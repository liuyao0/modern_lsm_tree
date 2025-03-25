#include <iostream>
#include <cstdint>
#include <string>
#include "KVStoreTest.h"
class CorrectnessTest : public KVStoreTest {
private:
	const uint64_t SIMPLE_TEST_MAX = 512;
	const uint64_t LARGE_TEST_MAX = 1024 * 6;

	void regular_test(uint64_t max)
	{
		uint64_t i;
		// Test a single key
		KVSTORE_EXPECT(not_found, store.get(1));
		store.put(1, "SE");
		KVSTORE_EXPECT("SE", store.get(1));
		KVSTORE_EXPECT(true, store.del(1));
		KVSTORE_EXPECT(not_found, store.get(1));
		KVSTORE_EXPECT(false, store.del(1));

		phase();

		// Test multiple key-value pairs
		for (i = 0; i < max; ++i) {
			store.put(i, std::string(i+1, 's'));
			KVSTORE_EXPECT(std::string(i+1, 's'), store.get(i));
		}
		phase();


		// Test after all insertions
		for (i = 0; i < max; ++i)
		{
			KVSTORE_EXPECT(std::string(i+1, 's'), store.get(i));
		}
		phase();

		// Test deletions
		for (i = 0; i < max; i++)
		{
			if(i&1)
				store.put(i,std::string(i+1,'t'));
			else
				KVSTORE_EXPECT(true, store.del(i));
			//  if(std::string(2955, 's')!=store.get(2954))
			// 	 	std::cout<<"!!!!"<<std::endl;
		}

		for (i = 0; i < max; ++i)
			KVSTORE_EXPECT((i & 1) ? std::string(i+1, 't') : not_found,
			       store.get(i));

		for (i = 1; i < max; ++i)
			KVSTORE_EXPECT(i & 1, store.del(i));

		phase();

		report();
	}

public:
	CorrectnessTest(const std::string &dir, bool v=true) : KVStoreTest(dir, v)
	{
	}

	void start_test(void *args = NULL) override
	{
		std::cout << "KVStore Correctness Test" << std::endl;

		std::cout << "[Simple Test]" << std::endl;
		regular_test(SIMPLE_TEST_MAX);

		std::cout << "[Large Test]" << std::endl;
		regular_test(LARGE_TEST_MAX);
	}
};

int main(int argc, char *argv[])
{
	bool verbose = (argc == 2 && std::string(argv[1]) == "-v");

	std::cout << "Usage: " << argv[0] << " [-v]" << std::endl;
	std::cout << "  -v: print extra info for failed tests [currently ";
	std::cout << (verbose ? "ON" : "OFF")<< "]" << std::endl;
	std::cout << std::endl;
	std::cout.flush();

	CorrectnessTest test("./db", verbose);

	test.start_test();

	return 0;
}
