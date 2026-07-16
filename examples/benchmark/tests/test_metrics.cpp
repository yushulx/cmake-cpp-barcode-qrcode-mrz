#include "test_support.h"
#include "metrics.h"
#include "result_writer.h"
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace bench;

void testMetrics()
{
    const auto ci=wilsonInterval(50,100);
    CHECK(std::abs(ci.first-0.4038)<0.001);
    CHECK(std::abs(ci.second-0.5962)<0.001);
    CHECK(wilsonInterval(0,0).first==0.0);

    const auto root=std::filesystem::temp_directory_path()/"barber_result_writer_test";
    std::filesystem::remove_all(root);
    RawResultRecord record; record.sample.sample_id="sample"; record.decoder="decoder";
    appendResult(root/"results.jsonl",record);
    appendResult(root/"results.jsonl",record);
    std::ifstream input(root/"results.jsonl"); std::string line; int lines=0;
    while(std::getline(input,line))if(!line.empty())++lines;
    CHECK(lines==1);
    input.close();
    std::filesystem::remove_all(root);
}
