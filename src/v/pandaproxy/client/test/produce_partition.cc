#include "pandaproxy/client/produce_partition.h"

#include "kafka/errors.h"
#include "kafka/requests/produce_request.h"
#include "model/fundamental.h"
#include "model/record.h"
#include "pandaproxy/client/brokers.h"
#include "pandaproxy/client/configuration.h"
#include "pandaproxy/client/test/utils.h"

#include <seastar/testing/thread_test_case.hh>

#include <boost/test/tools/old/interface.hpp>

#include <exception>
#include <system_error>

namespace ppc = pandaproxy::client;

SEASTAR_THREAD_TEST_CASE(test_produce_partition_record_count) {
    std::vector<model::record_batch> consumed_batches;
    auto consumer = [&consumed_batches](model::record_batch&& batch) {
        consumed_batches.push_back(std::move(batch));
    };

    // large
    ppc::shard_local_cfg().produce_batch_size_bytes.set_value(1024);
    // configuration under test
    ppc::shard_local_cfg().produce_batch_record_count.set_value(3);

    ppc::produce_partition producer(consumer);

    auto c_res0_fut = producer.produce(make_batch(model::offset(0), 2));
    auto c_res1_fut = producer.produce(make_batch(model::offset(2), 1));
    producer.handle_response(kafka::produce_response::partition{
      .id{model::partition_id{42}},
      .error = kafka::error_code::none,
      .base_offset{model::offset{0}}});

    BOOST_REQUIRE_EQUAL(consumed_batches.size(), 1);
    BOOST_REQUIRE_EQUAL(consumed_batches[0].record_count(), 3);
    auto c_res0 = c_res0_fut.get0();
    BOOST_REQUIRE_EQUAL(c_res0.base_offset, model::offset{0});
    auto c_res1 = c_res1_fut.get0();
    BOOST_REQUIRE_EQUAL(c_res1.base_offset, model::offset{2});

    auto c_res2_fut = producer.produce(make_batch(model::offset(3), 3));
    producer.handle_response(kafka::produce_response::partition{
      .id{model::partition_id{42}},
      .error = kafka::error_code::none,
      .base_offset{model::offset{3}}});

    BOOST_REQUIRE_EQUAL(consumed_batches.size(), 2);
    BOOST_REQUIRE_EQUAL(consumed_batches[1].record_count(), 3);
    auto c_res2 = c_res2_fut.get0();
    BOOST_REQUIRE_EQUAL(c_res2.base_offset, model::offset{3});
}
