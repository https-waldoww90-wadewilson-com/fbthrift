/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/portability/GFlags.h>
#include <folly/portability/GTest.h>

#include <thrift/lib/cpp2/async/RocketClientChannel.h>
#include <thrift/lib/cpp2/transport/core/testutil/MockCallback.h>
#include <thrift/lib/cpp2/transport/core/testutil/TransportCompatibilityTest.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerConnection.h>
#include <thrift/lib/cpp2/transport/rocket/server/ThriftRocketServerHandler.h>
#include <thrift/lib/cpp2/transport/rsocket/server/ManagedRSocketConnection.h>
#include <thrift/lib/cpp2/transport/rsocket/server/RSRoutingHandler.h>

DECLARE_int32(num_client_connections);
DECLARE_string(transport); // ConnectionManager depends on this flag.

namespace apache {
namespace thrift {

using namespace testutil::testservice;
using namespace apache::thrift::transport;

class RSCompatibilityTest
    : public testing::Test,
      public testing::WithParamInterface<bool /* useRocketClient */> {
 public:
  RSCompatibilityTest() {
    FLAGS_transport = "rocket";

    compatibilityTest_ = std::make_unique<TransportCompatibilityTest>();
    compatibilityTest_->addRoutingHandler(
        std::make_unique<apache::thrift::RSRoutingHandler>());
    compatibilityTest_->startServer();
  }

 protected:
  std::unique_ptr<TransportCompatibilityTest> compatibilityTest_;
};

TEST_P(RSCompatibilityTest, RequestResponse_Simple) {
  compatibilityTest_->TestRequestResponse_Simple();
}

TEST_P(RSCompatibilityTest, RequestResponse_Sync) {
  compatibilityTest_->TestRequestResponse_Sync();
}

TEST_P(RSCompatibilityTest, RequestResponse_Destruction) {
  compatibilityTest_->TestRequestResponse_Destruction();
}

TEST_P(RSCompatibilityTest, RequestResponse_MultipleClients) {
  compatibilityTest_->TestRequestResponse_MultipleClients();
}

TEST_P(RSCompatibilityTest, RequestResponse_ExpectedException) {
  compatibilityTest_->TestRequestResponse_ExpectedException();
}

TEST_P(RSCompatibilityTest, RequestResponse_UnexpectedException) {
  compatibilityTest_->TestRequestResponse_UnexpectedException();
}

// Warning: This test may be flaky due to use of timeouts.
TEST_P(RSCompatibilityTest, RequestResponse_Timeout) {
  compatibilityTest_->TestRequestResponse_Timeout();
}

TEST_P(RSCompatibilityTest, DefaultTimeoutValueTest) {
  compatibilityTest_->connectToServer([](auto client) {
    // Opts with no timeout value
    RpcOptions opts;

    // Ok to sleep for 100msec
    auto cb = std::make_unique<MockCallback>(false, false);
    client->sleep(opts, std::move(cb), 100);

    /* Sleep to give time for all callbacks to be completed */
    /* sleep override */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto* channel = dynamic_cast<ClientChannel*>(client->getChannel());
    EXPECT_TRUE(channel);
    channel->getEventBase()->runInEventBaseThreadAndWait([&]() {
      channel->setTimeout(1); // 1ms
    });

    // Now it should timeout
    cb = std::make_unique<MockCallback>(false, true);
    client->sleep(opts, std::move(cb), 100);

    /* Sleep to give time for all callbacks to be completed */
    /* sleep override */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  });
}

TEST_P(RSCompatibilityTest, RequestResponse_Header) {
  compatibilityTest_->TestRequestResponse_Header();
}

TEST_P(RSCompatibilityTest, RequestResponse_Header_Load) {
  compatibilityTest_->TestRequestResponse_Header_Load();
}

TEST_P(RSCompatibilityTest, RequestResponse_Header_ExpectedException) {
  compatibilityTest_->TestRequestResponse_Header_ExpectedException();
}

TEST_P(RSCompatibilityTest, RequestResponse_Header_UnexpectedException) {
  compatibilityTest_->TestRequestResponse_Header_UnexpectedException();
}

TEST_P(RSCompatibilityTest, RequestResponse_Saturation) {
  compatibilityTest_->connectToServer([this](auto client) {
    EXPECT_CALL(*compatibilityTest_->handler_.get(), add_(3)).Times(2);
    // note that no EXPECT_CALL for add_(5)
    auto* channel = dynamic_cast<RocketClientChannel*>(client->getChannel());
    ASSERT_NE(nullptr, channel);

    channel->getEventBase()->runInEventBaseThreadAndWait(
        [&]() { channel->setMaxPendingRequests(0u); });
    EXPECT_THROW(client->future_add(5).get(), TTransportException);

    channel->getEventBase()->runInEventBaseThreadAndWait(
        [&]() { channel->setMaxPendingRequests(1u); });

    EXPECT_EQ(3, client->future_add(3).get());
    EXPECT_EQ(6, client->future_add(3).get());
  });
}

TEST_P(RSCompatibilityTest, RequestResponse_Connection_CloseNow) {
  compatibilityTest_->TestRequestResponse_Connection_CloseNow();
}

TEST_P(RSCompatibilityTest, RequestResponse_ServerQueueTimeout) {
  compatibilityTest_->TestRequestResponse_ServerQueueTimeout();
}

TEST_P(RSCompatibilityTest, RequestResponse_ResponseSizeTooBig) {
  compatibilityTest_->TestRequestResponse_ResponseSizeTooBig();
}

TEST_P(RSCompatibilityTest, RequestResponse_Checksumming) {
  compatibilityTest_->TestRequestResponse_Checksumming();
}

TEST_P(RSCompatibilityTest, RequestResponse_CompressRequest) {
  compatibilityTest_->connectToServer([this](auto client) {
    EXPECT_CALL(*compatibilityTest_->handler_.get(), hello_(testing::_));
    auto* channel = dynamic_cast<RocketClientChannel*>(client->getChannel());
    ASSERT_NE(nullptr, channel);

    // set the channel to compress requests
    channel->setNegotiatedCompressionAlgorithm(CompressionAlgorithm::ZSTD);
    channel->setAutoCompressSizeLimit(0);

    std::string name("snoopy");
    auto result =
        client->future_hello(RpcOptions().setEnableChecksum(true), name).get();
    EXPECT_EQ("Hello, snoopy", result);
  });
}

TEST_P(RSCompatibilityTest, Oneway_Simple) {
  compatibilityTest_->TestOneway_Simple();
}

TEST_P(RSCompatibilityTest, Oneway_WithDelay) {
  compatibilityTest_->TestOneway_WithDelay();
}

TEST_P(RSCompatibilityTest, Oneway_Saturation) {
  compatibilityTest_->connectToServer([this](auto client) {
    EXPECT_CALL(*compatibilityTest_->handler_.get(), addAfterDelay_(100, 5));
    EXPECT_CALL(*compatibilityTest_->handler_.get(), addAfterDelay_(50, 5));

    if (auto* channel =
            dynamic_cast<RocketClientChannel*>(client->getChannel())) {
      channel->getEventBase()->runInEventBaseThreadAndWait(
          [&]() { channel->setMaxPendingRequests(0u); });
      EXPECT_THROW(
          client->future_addAfterDelay(0, 5).get(), TTransportException);

      // the first call is not completed as the connection was saturated
      channel->getEventBase()->runInEventBaseThreadAndWait(
          [&]() { channel->setMaxPendingRequests(1u); });
    } else {
      FAIL() << "Test run with unexpected channel type";
    }

    // Client should be able to issue both of these functions as
    // SINGLE_REQUEST_NO_RESPONSE doesn't need to wait for server response
    client->future_addAfterDelay(100, 5).get();
    client->future_addAfterDelay(50, 5).get(); // TODO: H2 fails in this call.
  });
}

TEST_P(RSCompatibilityTest, Oneway_UnexpectedException) {
  compatibilityTest_->TestOneway_UnexpectedException();
}

TEST_P(RSCompatibilityTest, Oneway_Connection_CloseNow) {
  compatibilityTest_->TestOneway_Connection_CloseNow();
}

TEST_P(RSCompatibilityTest, Oneway_ServerQueueTimeout) {
  compatibilityTest_->TestOneway_ServerQueueTimeout();
}

TEST_P(RSCompatibilityTest, Oneway_Checksumming) {
  compatibilityTest_->TestOneway_Checksumming();
}

TEST_P(RSCompatibilityTest, RequestContextIsPreserved) {
  compatibilityTest_->TestRequestContextIsPreserved();
}

TEST_P(RSCompatibilityTest, BadPayload) {
  compatibilityTest_->TestBadPayload();
}

TEST_P(RSCompatibilityTest, EvbSwitch) {
  compatibilityTest_->TestEvbSwitch();
}

TEST_P(RSCompatibilityTest, EvbSwitch_Failure) {
  compatibilityTest_->TestEvbSwitch_Failure();
}

TEST_P(RSCompatibilityTest, CloseCallback) {
  compatibilityTest_->TestCloseCallback();
}

TEST_P(RSCompatibilityTest, ConnectionStats) {
  compatibilityTest_->TestConnectionStats();
}

TEST_P(RSCompatibilityTest, ObserverSendReceiveRequests) {
  compatibilityTest_->TestObserverSendReceiveRequests();
}

TEST_P(RSCompatibilityTest, ConnectionContext) {
  compatibilityTest_->TestConnectionContext();
}

TEST_P(RSCompatibilityTest, ClientIdentityHook) {
  compatibilityTest_->TestClientIdentityHook();
}

INSTANTIATE_TEST_CASE_P(
    RSCompatibilityTests,
    RSCompatibilityTest,
    testing::Values(false, true));

/**
 * This is a test class with customized RoutingHandler. The main purpose is to
 * set compression on the server-side and test the behavior.
 */
class RSCompatibilityTest2
    : public testing::Test,
      public testing::WithParamInterface<bool /* useRocketClient */> {
  class TestRoutingHandler : public RSRoutingHandler {
   public:
    TestRoutingHandler() = default;
    TestRoutingHandler(const TestRoutingHandler&) = delete;
    TestRoutingHandler& operator=(const TestRoutingHandler&) = delete;
    void handleConnection(
        wangle::ConnectionManager* connectionManager,
        folly::AsyncTransportWrapper::UniquePtr sock,
        folly::SocketAddress const* address,
        wangle::TransportInfo const& /*tinfo*/,
        std::shared_ptr<Cpp2Worker> worker) override {
      auto sockPtr = sock.get();

      wangle::ManagedConnection* connection = nullptr;
      auto* const server = worker->getServer();
      if (server->isRocketServerEnabled()) {
        connection = new rocket::RocketServerConnection(
            std::move(sock),
            std::make_shared<rocket::ThriftRocketServerHandler>(
                worker, *address, sockPtr),
            server->getStreamExpireTime());
        // set compression algorithm to be used on this connection
        static_cast<rocket::RocketServerConnection*>(connection)
            ->setNegotiatedCompressionAlgorithm(CompressionAlgorithm::ZSTD);
        // set minCompressBytes
        static_cast<rocket::RocketServerConnection*>(connection)
            ->setMinCompressBytes(server->getMinCompressBytes());
      } else {
        connection = new ManagedRSocketConnection(
            std::move(sock),
            [sockPtr, worker = worker, clientAddress = folly::copy(*address)](
                auto&) mutable {
              DCHECK(worker->getServer());
              DCHECK(worker->getServer()->getCpp2Processor());
              // RSResponder will be created per client connection. It will use
              // the current Observer of the server.
              return std::make_shared<RSResponder>(
                  std::move(worker), clientAddress, sockPtr);
            });
        // set compression algorithm to be used on this connection
        static_cast<ManagedRSocketConnection*>(connection)
            ->setNegotiatedCompressionAlgorithm(CompressionAlgorithm::ZSTD);
        // set minCompressBytes
        static_cast<ManagedRSocketConnection*>(connection)
            ->setMinCompressBytes(server->getMinCompressBytes());
      }

      connectionManager->addConnection(connection);

      if (auto* observer = server->getObserver()) {
        observer->connAccepted();
        observer->activeConnections(
            connectionManager->getNumConnections() *
            server->getNumIOWorkerThreads());
      }
    }
  };

 public:
  RSCompatibilityTest2() {
    FLAGS_transport = "rocket";

    compatibilityTest_ = std::make_unique<TransportCompatibilityTest>();
    compatibilityTest_->addRoutingHandler(
        std::make_unique<TestRoutingHandler>());
    compatibilityTest_->startServer();
  }

 protected:
  std::unique_ptr<TransportCompatibilityTest> compatibilityTest_;
};

TEST_P(RSCompatibilityTest2, RequestResponse_CompressRequestResponse) {
  compatibilityTest_->connectToServer([this](auto client) {
    EXPECT_CALL(*compatibilityTest_->handler_.get(), hello_(testing::_));
    auto* channel = dynamic_cast<RocketClientChannel*>(client->getChannel());
    ASSERT_NE(nullptr, channel);

    // set the channel to compress requests
    channel->setNegotiatedCompressionAlgorithm(CompressionAlgorithm::ZSTD);
    channel->setAutoCompressSizeLimit(0);

    std::string name("snoopy");
    auto result =
        client->future_hello(RpcOptions().setEnableChecksum(true), name).get();
    EXPECT_EQ("Hello, snoopy", result);
  });
}

INSTANTIATE_TEST_CASE_P(
    RSCompatibilityTests,
    RSCompatibilityTest2,
    testing::Values(false, true));
} // namespace thrift
} // namespace apache
