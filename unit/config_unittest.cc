#include "evpaxos.h"
#include "gtest/gtest.h"

TEST(ConfigTest, TooManyProcesses) {
	struct evpaxos_config* config;
	config = evpaxos_config_read("config/too-many.conf");
	ASSERT_EQ(NULL, config);
}

TEST(ConfigTest, Folder) {
	struct evpaxos_config* config;
	config = evpaxos_config_read("config/");
	ASSERT_EQ(NULL, config);
	config = evpaxos_config_read("config");
	ASSERT_EQ(NULL, config);
}

TEST(ConfigTest, Replicas) {
	struct evpaxos_config* config;
	config = evpaxos_config_read("config/replicas.conf");
	
	ASSERT_EQ(3, evpaxos_acceptor_count(config));
	
	ASSERT_EQ(8800, evpaxos_proposer_listen_port(config, 0));
	ASSERT_EQ(8801, evpaxos_proposer_listen_port(config, 1));
	ASSERT_EQ(8802, evpaxos_proposer_listen_port(config, 2));
	
	ASSERT_EQ(8800, evpaxos_acceptor_listen_port(config, 0));
	ASSERT_EQ(8801, evpaxos_acceptor_listen_port(config, 1));
	ASSERT_EQ(8802, evpaxos_acceptor_listen_port(config, 2));
}
