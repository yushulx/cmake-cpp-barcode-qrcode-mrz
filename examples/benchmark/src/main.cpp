#include "barber_dataset.h"
#include "decoder_adapter.h"
#include "hash.h"
#include "image_loader.h"
#include "matcher.h"
#include "result_writer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {

using Options = std::map<std::string,std::string>;

Options parseOptions(int argc,char** argv,int start)
{
    Options result;
    for(int i=start;i<argc;++i){
        std::string key=argv[i];
        if(key.rfind("--",0)!=0)throw std::runtime_error("unexpected argument: "+key);
        if(i+1>=argc)throw std::runtime_error("missing value for "+key);
        result[key]=argv[++i];
    }
    return result;
}

std::string require(const Options& options,const std::string& key)
{
    auto it=options.find(key);
    if(it==options.end())throw std::runtime_error("required option: "+key);
    return it->second;
}

std::string licenseKey(const Options& options)
{
    if(const char* value=std::getenv("DYNAMSOFT_LICENSE_KEY"))return value;
    const auto path=require(options,"--license-key-file");
    std::ifstream input(path);std::string value;std::getline(input,value);
    if(value.empty())throw std::runtime_error("Dynamsoft license key file is empty: "+path);
    return value;
}

int audit(const Options& options)
{
    bench::BarberDataset dataset;
    const auto summary=dataset.audit(require(options,"--images"),require(options,"--annotations"),
                                     options.count("--output")?options.at("--output"):"manifests");
    std::cout<<"annotation_files="<<summary.annotation_files
             <<" manifest_images="<<summary.manifest_images
             <<" source_images="<<summary.source_images
             <<" annotations="<<summary.annotations
             <<" eligible="<<summary.eligible_annotations
             <<" excluded_missing_or_generic="<<summary.missing_payload_or_generic
             <<" invalid="<<summary.invalid_annotations
             <<" missing_images="<<summary.missing_images
             <<" unannotated_images="<<summary.unannotated_images
             <<" duplicate_records="<<summary.duplicate_image_records
             <<" excluded_images_without_ground_truth="<<summary.excluded_images_without_ground_truth
             <<" benchmark_annotations="<<summary.benchmark_annotations
             <<" max_barcodes="<<summary.dataset_max_barcodes<<'\n';
    return summary.missing_images?2:0;
}

std::vector<bench::MatchItem> errorMatches(const bench::ManifestRecord& sample,bench::Outcome outcome)
{
    std::vector<bench::MatchItem> result;
    for(std::size_t i=0;i<sample.ground_truth.size();++i)
        if(sample.ground_truth[i].decode_eligible)result.push_back({i,std::nullopt,outcome});
    return result;
}

int execute(const Options& options,bool smoke)
{
    const fs::path image_root=require(options,"--images");
    const fs::path manifest=require(options,"--manifest");
    const fs::path output=require(options,"--output");
    const fs::path dbr_config=options.count("--dbr-config")?options.at("--dbr-config"):"";
    const std::string dbr_template_label=options.count("--dbr-template")?options.at("--dbr-template"):"ReadBarcodes_Default";
    auto samples=bench::BarberDataset::readManifest(manifest);
    if(smoke&&samples.size()!=10)throw std::runtime_error("smoke manifest must contain exactly 10 images");
    if(samples.empty())throw std::runtime_error("manifest contains no benchmark images");
    for(const auto& sample:samples){
        if(sample.ground_truth.empty())throw std::runtime_error("manifest image has no ground truth: "+sample.relative_path);
        if(std::any_of(sample.ground_truth.begin(),sample.ground_truth.end(),[](const auto& gt){return !gt.decode_eligible;}))
            throw std::runtime_error("manifest contains excluded ground truth: "+sample.relative_path);
    }
    const int repetitions=options.count("--repetitions")?std::stoi(options.at("--repetitions")):1;
    int max_symbols=1;
    for(const auto& sample:samples)max_symbols=std::max(max_symbols,static_cast<int>(sample.ground_truth.size()));
    auto zxing=bench::createZxingDecoder(max_symbols);
    auto dbr=bench::createDynamsoftDecoder(dbr_config.string(),dbr_template_label,licenseKey(options),max_symbols);
    std::cout<<"ZXing-C++="<<zxing->version()<<" DBR="<<dbr->version()
             <<" images="<<samples.size()<<" repetitions="<<repetitions<<'\n';
    fs::create_directories(output);
    const auto jsonl=output/"results.jsonl";
    auto completed=bench::completedKeys(jsonl);
    const auto manifest_hash=bench::sha256File(manifest);
    const auto zxing_config_hash=bench::sha256File(options.count("--zxing-config")?options.at("--zxing-config"):"configs/zxing_all_supported.json");
    const auto dbr_config_hash=dbr_config.empty()?std::string("dbr-template:")+dbr_template_label:bench::sha256File(dbr_config);

    for(int repetition=0;repetition<repetitions;++repetition){
        std::size_t sample_index=0;
        for(const auto& sample:samples){
            const auto zxing_key=bench::recordKey(sample.sample_id,zxing->name(),repetition);
            const auto dbr_key=bench::recordKey(sample.sample_id,dbr->name(),repetition);
            if(completed.count(zxing_key)&&completed.count(dbr_key)){
                ++sample_index;
                continue;
            }
            bench::ImageBuffer image;std::string error;
            const auto load_begin=std::chrono::steady_clock::now();
            const bool loaded=bench::loadImage(image_root/sample.relative_path,image,error);
            const auto load_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-load_begin).count();
            std::array<bench::IDecoderAdapter*,2> decoders={zxing.get(),dbr.get()};
            std::mt19937 order(static_cast<unsigned>(std::hash<std::string>{}(sample.sample_id)^static_cast<std::size_t>(repetition)));
            if(order()&1)std::swap(decoders[0],decoders[1]);
            for(auto* decoder:decoders){
                const auto key=bench::recordKey(sample.sample_id,decoder->name(),repetition);
                if(completed.count(key))continue;
                bench::DecodeRun run;
                if(loaded)run=decoder->decode(image);else run.error="input_pipeline_error: "+error;
                bench::RawResultRecord record;
                record.protocol="protocol-v1";record.manifest_sha256=manifest_hash;
                record.sample=sample;record.decoder=decoder->name();record.decoder_version=decoder->version();
                record.config_sha256=decoder==zxing.get()?zxing_config_hash:dbr_config_hash;
                record.repetition=repetition;record.image_load_ns=load_ns;record.run=std::move(run);
                if(record.run.error){
                    const auto outcome=loaded?bench::Outcome::DecoderError:bench::Outcome::InputPipelineError;
                    record.matches=errorMatches(record.sample,outcome);
                }else{
                    record.matches=bench::matchResults(record.sample.ground_truth,record.run.results,record.decoder);
                }
                bench::appendResult(jsonl,record);completed.insert(key);
            }
            ++sample_index;
            if(sample_index%100==0||sample_index==samples.size())
                std::cout<<"repetition="<<(repetition+1)<<" progress="<<sample_index<<"/"<<samples.size()<<'\n'<<std::flush;
        }
    }
    const auto summary=output/"summary.json";
    const auto results_json=output/"results.json";
    bench::generateSummary(jsonl,summary);
    bench::generateResultsJson(jsonl,summary,results_json);
    std::cout<<"wrote "<<jsonl<<", "<<summary<<" and "<<results_json<<'\n';
    return 0;
}

void usage()
{
    std::cout
      <<"Usage:\n"
      <<"  barcode_benchmark audit --images DIR --annotations DIR [--output DIR]\n"
      <<"  barcode_benchmark smoke --images DIR --manifest FILE --output DIR --license-key-file FILE [--dbr-config FILE] [--dbr-template NAME] [--zxing-config FILE] [--repetitions N]\n"
      <<"  barcode_benchmark run   --images DIR --manifest FILE --output DIR --license-key-file FILE [--dbr-config FILE] [--dbr-template NAME] [--zxing-config FILE] [--repetitions N]\n";
}
}

int main(int argc,char** argv)
{
    try{
        if(argc<2){usage();return 1;}
        const std::string command=argv[1];const auto options=parseOptions(argc,argv,2);
        if(command=="audit")return audit(options);
        if(command=="smoke")return execute(options,true);
        if(command=="run")return execute(options,false);
        usage();return 1;
    }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
