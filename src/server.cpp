// Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gflags/gflags.h>              // DEFINE_*
#include <brpc/controller.h>       // brpc::Controller
#include <brpc/server.h>           // brpc::Server
#include <braft/raft.h>                  // braft::Node braft::StateMachine
#include <braft/storage.h>               // braft::SnapshotWriter
#include <braft/util.h>                  // braft::AsyncClosureGuard
#include <braft/protobuf_file.h>         // braft::ProtoBufFile
#include "kvstore.pb.h"                 // CounterService
#include "KVStore.h"
#include <string>

DEFINE_bool(check_term, true, "Check if the leader changed to another term");
DEFINE_bool(disable_cli, false, "Don't allow raft_cli access this node");
DEFINE_bool(log_applied_task, false, "Print notice log when a task is applied");
DEFINE_int32(election_timeout_ms, 5000, 
            "Start election in such milliseconds if disconnect with the leader");
DEFINE_int32(port, 8100, "Listen port of this peer");
DEFINE_int32(snapshot_interval, 30, "Interval between each snapshot");
DEFINE_string(conf, "", "Initial configuration of the replication group");
DEFINE_string(data_path, "./data", "Path of data stored on");
DEFINE_string(db_path, "./db", "Path of sstable stored on");
DEFINE_string(group, "Counter", "Id of the replication group");



namespace KVS {
class KVStoreStateMachine;

// Implements Closure which encloses RPC stuff
class KVStoreClosure : public braft::Closure {
public:
    KVStoreClosure(KVStoreStateMachine* kvsm,
                    const google::protobuf::Message* request,
                    KVStoreResponse* response,
                    google::protobuf::Closure* done)
        : _kvsm(kvsm)
        , _request(request)
        , _response(response)
        , _done(done) {}

    ~KVStoreClosure() override {}

    const google::protobuf::Message* request() const { return _request; }
    KVStoreResponse* response() const { return _response; }
    void Run();

private:
    KVStoreStateMachine* _kvsm;
    const google::protobuf::Message* _request;
    KVStoreResponse* _response;
    google::protobuf::Closure* _done;
};

// Implementation of example::Counter as a braft::StateMachine.
class KVStoreStateMachine : public braft::StateMachine {
public:
    enum KVStoreOpType {
        OP_UNKNOWN = 0,
        OP_GET = 1,
        OP_PUT = 2,
        OP_DEL = 3,
    };
private:
    braft::Node* volatile _node;
    butil::atomic<int64_t> _leader_term;
    unique_ptr<KVStore<int64_t, string>> kvstore_ptr;
public:
    KVStoreStateMachine()
        : _node(nullptr)
        , _leader_term(-1)
        , kvstore_ptr(make_unique<KVStore<int64_t, string>>(FLAGS_db_path))
    {}

    ~KVStoreStateMachine() override {
        delete _node;
    }

    // Starts this node
    int start() {
        butil::ip_t loopback_ip;
        butil::str2ip("127.0.0.1", &loopback_ip);
        const butil::EndPoint addr(loopback_ip, FLAGS_port);

        if (!butil::CreateDirectory(butil::FilePath(FLAGS_data_path))) {
            LOG(ERROR) << "Fail to create directory " << FLAGS_data_path;
            return -1;
        }

        if (!butil::CreateDirectory(butil::FilePath(FLAGS_db_path))) {
            LOG(ERROR) << "Fail to create directory " << FLAGS_data_path;
            return -1;
        }

        braft::NodeOptions node_options;
        if (node_options.initial_conf.parse_from(FLAGS_conf) != 0) {
            LOG(ERROR) << "Fail to parse configuration `" << FLAGS_conf << '\'';
            return -1;
        }
        node_options.election_timeout_ms = FLAGS_election_timeout_ms;
        node_options.fsm = this;
        node_options.node_owns_fsm = false;
        node_options.snapshot_interval_s = FLAGS_snapshot_interval;
        std::string prefix = "local://" + FLAGS_data_path;
        node_options.log_uri = prefix + "/log";
        node_options.raft_meta_uri = prefix + "/raft_meta";
        node_options.snapshot_uri = prefix + "/snapshot";
        node_options.disable_cli = FLAGS_disable_cli;
        braft::Node* node = new braft::Node(FLAGS_group, braft::PeerId(addr));
        if (node->init(node_options) != 0) {
            LOG(ERROR) << "Fail to init raft node";
            delete node;
            return -1;
        }
        _node = node;
        return 0;
    }

    void get(const ::KVS::GetRequest* request,
         ::KVS::KVStoreResponse* response,
         ::google::protobuf::Closure* done)
    {
        apply(OP_GET, request, response, done);
    }

    void put(const ::KVS::PutRequest* request,
         ::KVS::KVStoreResponse* response,
         ::google::protobuf::Closure* done)
    {
        apply(OP_PUT, request, response, done);
    }

    void del(const ::KVS::DelRequest* request,
         ::KVS::KVStoreResponse* response,
         ::google::protobuf::Closure* done)
    {
        apply(OP_DEL, request, response, done);
    }

    bool is_leader() const
    { return _leader_term.load(butil::memory_order_acquire) > 0; }

    // Shut this node down.
    void shutdown() {
        if (_node) {
            _node->shutdown(nullptr);
        }
    }

    // Blocking this thread until the node is eventually down.
    void join() {
        if (_node) {
            _node->join();
        }
    }

private:
friend class KVStoreClosure;
    void apply(KVStoreOpType type, const google::protobuf::Message* request,
              KVStoreResponse* response, google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        // Serialize request to the replicated write-ahead-log so that all the
        // peers in the group receive this request as well.
        // Notice that _value can't be modified in this routine otherwise it
        // will be inconsistent with others in this group.

        // Serialize request to IOBuf
        const int64_t term = _leader_term.load(butil::memory_order_relaxed);
        if (term < 0) {
            return redirect(response);
        }
        butil::IOBuf log;
        log.push_back((uint8_t)type);
        butil::IOBufAsZeroCopyOutputStream wrapper(&log);
        if (!request->SerializeToZeroCopyStream(&wrapper)) {
            LOG(ERROR) << "Fail to serialize request";
            response->set_success(false);
            return;
        }
        // Apply this log as a braft::Task
        braft::Task task;
        task.data = &log;
        // This callback would be iovoked when the task actually excuted or
        // fail
        task.done = new KVStoreClosure(this, request, response,
                                      done_guard.release());
        if (FLAGS_check_term) {
            // ABA problem can be avoid if expected_term is set
            task.expected_term = term;
        }
        // Now the task is applied to the group, waiting for the result.
        return _node->apply(task);
    }

    void redirect(KVStoreResponse* response) {
        response->set_success(false);
        if (_node) {
            braft::PeerId leader = _node->leader_id();
            if (!leader.is_empty()) {
                response->set_redirect(leader.to_string());
            }
        }
    }

    // @braft::StateMachine
    void on_apply(braft::Iterator& iter) {
        for (; iter.valid(); iter.next()) {
            // This guard helps invoke iter.done()->Run() asynchronously to
            // avoid that callback blocks the StateMachine.
            braft::AsyncClosureGuard done_guard(iter.done());

            // Parse data
            butil::IOBuf data = iter.data();
            // Fetch the type of operation from the leading byte.
            uint8_t type = OP_UNKNOWN;
            data.cutn(&type, sizeof(uint8_t));

            KVStoreClosure* c = nullptr;
            if (iter.done()) {
                c = dynamic_cast<KVStoreClosure*>(iter.done());
            }

            const google::protobuf::Message* request = c ? c->request() : NULL;
            KVStoreResponse r;
            KVStoreResponse* response = c ? c->response() : &r;
            const char* op = nullptr;
            // Execute the operation according to type
            switch (type) {
            case OP_GET:
                op = "get";
                get_impl(data, request, response);
                break;
            case OP_PUT:
                op = "exchange";
                put_impl(data, request, response);
                break;
            case OP_DEL:
                op = "cas";
                del_impl(data, request, response);
                break;
            default:
                CHECK(false) << "Unknown type=" << static_cast<int>(type);
                break;
            }

            // The purpose of following logs is to help you understand the way
            // this StateMachine works.
            // Remove these logs in performance-sensitive servers.
            LOG_IF(INFO, FLAGS_log_applied_task)
                    << "Handled operation " << op
                    << " on key=" << response->key()
                    << " at log_index=" << iter.index()
                    << " success=" << response->success();
        }
    }

    void on_leader_start(int64_t term) {
        _leader_term.store(term, butil::memory_order_release);
        LOG(INFO) << "Node becomes leader";
    }
    void on_leader_stop(const butil::Status& status) {
        _leader_term.store(-1, butil::memory_order_release);
        LOG(INFO) << "Node stepped down : " << status;
    }

    void on_shutdown() {
        LOG(INFO) << "This node is down";
    }
    void on_error(const ::braft::Error& e) {
        LOG(ERROR) << "Met raft error " << e;
    }
    void on_configuration_committed(const ::braft::Configuration& conf) {
        LOG(INFO) << "Configuration of this group is " << conf;
    }
    void on_stop_following(const ::braft::LeaderChangeContext& ctx) {
        LOG(INFO) << "Node stops following " << ctx;
    }
    void on_start_following(const ::braft::LeaderChangeContext& ctx) {
        LOG(INFO) << "Node start following " << ctx;
    }
    // end of @braft::StateMachine

    void get_impl(const butil::IOBuf& data,
                   const google::protobuf::Message* request,
                   KVStoreResponse* response) {
        int64_t key = 0;
        if (request) {
            // This task is applied by this node, get value from this
            // closure to avoid additional parsing.
            key = dynamic_cast<const GetRequest*>(request)->key();
        } else {
            butil::IOBufAsZeroCopyInputStream wrapper(data);
            GetRequest req;
            CHECK(req.ParseFromZeroCopyStream(&wrapper));
            key = req.key();
        }
        unique_ptr<string> value_ptr = kvstore_ptr -> get(key);

        response->set_success(true);
        response->set_key(key);
        response->set_value(value_ptr == nullptr ? "" : std::move(*value_ptr));
    }

    void put_impl(const butil::IOBuf& data,
               const google::protobuf::Message* request,
               KVStoreResponse* response) {
        int64_t key = 0;
        string value;
        if (request) {
            // This task is applied by this node, get value from this
            // closure to avoid additional parsing.
            key = dynamic_cast<const PutRequest*>(request)->key();
            value = dynamic_cast<const PutRequest*>(request)->value();
        } else {
            butil::IOBufAsZeroCopyInputStream wrapper(data);
            PutRequest req;
            CHECK(req.ParseFromZeroCopyStream(&wrapper));
            key = req.key();
            value = req.value();
        }
        kvstore_ptr -> put(key, value);
        response->set_success(true);
        response->set_key(key);
    }

    void del_impl(const butil::IOBuf& data,
               const google::protobuf::Message* request,
               KVStoreResponse* response) {
        int64_t key = 0;
        if (request) {
            // This task is applied by this node, get value from this
            // closure to avoid additional parsing.
            key = dynamic_cast<const DelRequest*>(request)->key();
        } else {
            butil::IOBufAsZeroCopyInputStream wrapper(data);
            DelRequest req;
            CHECK(req.ParseFromZeroCopyStream(&wrapper));
            key = req.key();
        }
        kvstore_ptr -> del(key);
        response->set_success(true);
        response->set_key(key);
    }


};

void KVStoreClosure::Run() {
    // Auto delete this after Run()
    std::unique_ptr<KVStoreClosure> self_guard(this);
    // Repsond this RPC.
    brpc::ClosureGuard done_guard(_done);
    if (status().ok()) {
        return;
    }
    // Try redirect if this request failed.
    _kvsm->redirect(_response);
}

// Implements example::CounterService if you are using brpc.
class KVStoreServiceImpl : public KVStoreService {
public:
    explicit KVStoreServiceImpl(KVStoreStateMachine* kvsm) : _kvsm(kvsm) {}
    void get(::google::protobuf::RpcController* controller,
                   const ::KVS::GetRequest* request,
                   ::KVS::KVStoreResponse* response,
                   ::google::protobuf::Closure* done) {
        return _kvsm->get(request, response, done);
    }

    void put(::google::protobuf::RpcController* controller,
               const ::KVS::PutRequest* request,
               ::KVS::KVStoreResponse* response,
               ::google::protobuf::Closure* done) {
        return _kvsm->put(request, response, done);
    }

    void del(::google::protobuf::RpcController* controller,
               const ::KVS::DelRequest* request,
               ::KVS::KVStoreResponse* response,
               ::google::protobuf::Closure* done) {
        return _kvsm->del(request, response, done);
    }

private:
    KVStoreStateMachine* _kvsm;
};

}  // namespace example

int main(int argc, char* argv[]) {
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    butil::AtExitManager exit_manager;

    // Generally you only need one Server.
    brpc::Server server;
    KVS::KVStoreStateMachine kvsm;
    KVS::KVStoreServiceImpl service(&kvsm);

    // Add your service into RPC server
    if (server.AddService(&service,
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }
    // raft can share the same RPC server. Notice the second parameter, because
    // adding services into a running server is not allowed and the listen
    // address of this server is impossible to get before the server starts. You
    // have to specify the address of the server.
    if (braft::add_service(&server, FLAGS_port) != 0) {
        LOG(ERROR) << "Fail to add raft service";
        return -1;
    }

    // It's recommended to start the server before Counter is started to avoid
    // the case that it becomes the leader while the service is unreacheable by
    // clients.
    // Notice the default options of server is used here. Check out details from
    // the doc of brpc if you would like change some options;
    if (server.Start(FLAGS_port, NULL) != 0) {
        LOG(ERROR) << "Fail to start Server";
        return -1;
    }

    // It's ok to start Counter;
    if (kvsm.start() != 0) {
        LOG(ERROR) << "Fail to start Counter";
        return -1;
    }

    LOG(INFO) << "Counter service is running on " << server.listen_address();
    // Wait until 'CTRL-C' is pressed. then Stop() and Join() the service
    while (!brpc::IsAskedToQuit()) {
        sleep(1);
    }

    LOG(INFO) << "Counter service is going to quit";

    // Stop counter before server
    kvsm.shutdown();
    server.Stop(0);

    // Wait until all the processing tasks are over.
    kvsm.join();
    server.Join();
    return 0;
}
