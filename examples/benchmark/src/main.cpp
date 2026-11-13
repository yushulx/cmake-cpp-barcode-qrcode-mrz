#include "barber_dataset.h"
#include "decoder_adapter.h"
#include "hash.h"
#include "image_loader.h"
#include "matcher.h"
#include "result_writer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string readLicense(const Options& options,const char* env_name,
                        const std::string& file_option,const std::string& label)
{
    if(const char* value=std::getenv(env_name))return value;
    auto it=options.find(file_option);
    if(it==options.end())
        throw std::runtime_error(label+" requires "+file_option+" or the "+env_name+" environment variable");
    std::ifstream input(it->second);std::string value;std::getline(input,value);
    while(!value.empty()&&(value.back()=='\r'||value.back()=='\n'||value.back()==' '||value.back()=='\t'))
        value.pop_back();
    if(value.empty())throw std::runtime_error(label+" license key file is empty: "+it->second);
    return value;
}

std::vector<std::string> splitList(const std::string& value)
{
    std::vector<std::string> result;
    std::size_t begin=0;
    while(begin<=value.size()){
        const auto end=value.find(',',begin);
        auto item=value.substr(begin,end==std::string::npos?std::string::npos:end-begin);
        if(!item.empty())result.push_back(item);
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return result;
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

#ifdef BENCH_HAS_SCANDIT
constexpr const char* kDefaultDecoders="zxing-cpp,dynamsoft-dbr,scandit-linux";
#else
constexpr const char* kDefaultDecoders="zxing-cpp,dynamsoft-dbr";
#endif

int execute(const Options& options,bool smoke)
{
    const fs::path image_root=require(options,"--images");
    const fs::path manifest=require(options,"--manifest");
    const fs::path output=require(options,"--output");
    const fs::path dbr_config=options.count("--dbr-config")?options.at("--dbr-config"):"";
    const std::string dbr_template_label=options.count("--dbr-template")?options.at("--dbr-template"):"ReadBarcodes_Default";
    const std::string zxing_config_path=options.count("--zxing-config")?options.at("--zxing-config"):"configs/zxing_all_supported.json";
    const std::string scandit_config_path=options.count("--scandit-config")?options.at("--scandit-config"):"configs/scandit_all_supported.json";

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

    const auto requested=splitList(options.count("--decoders")?options.at("--decoders"):kDefaultDecoders);
    std::vector<std::unique_ptr<bench::IDecoderAdapter>> decoders;
    std::map<std::string,std::string> config_hashes;
    for(const auto& name:requested){
        if(name=="zxing-cpp"){
            decoders.push_back(bench::createZxingDecoder(max_symbols));
            config_hashes[name]=bench::sha256File(zxing_config_path);
        }else if(name=="dynamsoft-dbr"){
            decoders.push_back(bench::createDynamsoftDecoder(
                dbr_config.string(),dbr_template_label,
                readLicense(options,"DYNAMSOFT_LICENSE_KEY","--license-key-file","Dynamsoft"),
                options.count("--dbr-expected-count")?std::stoi(options.at("--dbr-expected-count")):-1));
            config_hashes[name]=dbr_config.empty()?std::string("dbr-template:")+dbr_template_label
                                                  :bench::sha256File(dbr_config);
        }else if(name=="scandit-linux"){
#ifdef BENCH_HAS_SCANDIT
            const std::string writable=options.count("--scandit-data-dir")
                ?options.at("--scandit-data-dir"):"/var/tmp/scandit";
            std::error_code mkdir_error;fs::create_directories(writable,mkdir_error);
            if(mkdir_error)throw std::runtime_error("cannot create Scandit data directory "+writable+": "+mkdir_error.message());
            decoders.push_back(bench::createScanditDecoder(
                scandit_config_path,
                readLicense(options,"SCANDIT_LICENSE_KEY","--scandit-license-key-file","Scandit"),
                writable,max_symbols));
            config_hashes[name]=bench::sha256File(scandit_config_path);
#else
            throw std::runtime_error("this build has no Scandit decoder (Linux SDK not found at configure time)");
#endif
        }else{
            throw std::runtime_error("unknown decoder: "+name);
        }
    }
    if(decoders.empty())throw std::runtime_error("--decoders selected no decoder");

    for(const auto& decoder:decoders){
        std::cout<<decoder->name()<<"="<<decoder->version();
        const auto note=decoder->configuration();
        if(!note.empty())std::cout<<" ("<<note<<")";
        std::cout<<'\n';
    }
    std::cout<<"images="<<samples.size()<<" repetitions="<<repetitions<<'\n';

    fs::create_directories(output);
    const auto jsonl=output/"results.jsonl";
    auto completed=bench::completedKeys(jsonl);
    const auto manifest_hash=bench::sha256File(manifest);

    // Optional contiguous slice of the manifest. Multiple processes can share
    // one results directory (appendResult is cross-process safe) while working
    // on disjoint slices, which keeps manifest hashes identical across records.
    const std::size_t start_index=options.count("--start-index")
        ?static_cast<std::size_t>(std::stoull(options.at("--start-index"))):0;
    const std::size_t end_index=options.count("--end-index")
        ?static_cast<std::size_t>(std::stoull(options.at("--end-index"))):samples.size();
    if(start_index>end_index||end_index>samples.size())
        throw std::runtime_error("invalid --start-index/--end-index");
    if(start_index||end_index!=samples.size())
        std::cout<<"manifest slice=["<<start_index<<","<<end_index<<")\n";

    for(int repetition=0;repetition<repetitions;++repetition){
        for(std::size_t sample_index=start_index;sample_index<end_index;++sample_index){
            const auto& sample=samples[sample_index];
            bool all_done=true;
            for(const auto& decoder:decoders)
                if(!completed.count(bench::recordKey(sample.sample_id,decoder->name(),repetition)))all_done=false;
            if(all_done)continue;

            bench::ImageBuffer image;std::string error;
            const auto load_begin=std::chrono::steady_clock::now();
            const bool loaded=bench::loadImage(image_root/sample.relative_path,image,error);
            const auto load_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-load_begin).count();
            std::vector<bench::IDecoderAdapter*> order;
            order.reserve(decoders.size());
            for(const auto& decoder:decoders)order.push_back(decoder.get());
            std::mt19937 rng(static_cast<unsigned>(std::hash<std::string>{}(sample.sample_id)^static_cast<std::size_t>(repetition)));
            std::shuffle(order.begin(),order.end(),rng);

            for(auto* decoder:order){
                const auto key=bench::recordKey(sample.sample_id,decoder->name(),repetition);
                if(completed.count(key))continue;
                bench::DecodeRun run;
                if(loaded)run=decoder->decode(image);else run.error="input_pipeline_error: "+error;
                bench::RawResultRecord record;
                record.protocol="protocol-v2";record.manifest_sha256=manifest_hash;
                record.sample=sample;record.decoder=decoder->name();record.decoder_version=decoder->version();
                record.config_sha256=config_hashes.at(decoder->name());
                record.repetition=repetition;record.image_load_ns=load_ns;record.run=std::move(run);
                if(record.run.error){
                    const auto outcome=loaded?bench::Outcome::DecoderError:bench::Outcome::InputPipelineError;
                    record.matches=errorMatches(record.sample,outcome);
                }else{
                    record.matches=bench::matchResults(record.sample.ground_truth,record.run.results,record.decoder);
                }
                bench::appendResult(jsonl,record);completed.insert(key);
            }
            if((sample_index+1)%100==0||sample_index+1==end_index)
                std::cout<<"repetition="<<(repetition+1)<<" progress="<<(sample_index+1)<<"/"<<samples.size()<<'\n'<<std::flush;
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
      <<"  barcode_benchmark smoke --images DIR --manifest FILE --output DIR --license-key-file FILE [--decoders LIST] [--scandit-license-key-file FILE] [--dbr-config FILE] [--dbr-template NAME] [--zxing-config FILE] [--scandit-config FILE] [--scandit-data-dir DIR] [--repetitions N]\n"
      <<"  barcode_benchmark run   --images DIR --manifest FILE --output DIR --license-key-file FILE [--decoders LIST] [--scandit-license-key-file FILE] [--dbr-config FILE] [--dbr-template NAME] [--zxing-config FILE] [--scandit-config FILE] [--scandit-data-dir DIR] [--repetitions N]\n"
      <<"Decoders: zxing-cpp, dynamsoft-dbr, scandit-linux (comma separated).\n"
      <<"Optional: --start-index N --end-index N to process one manifest slice per process.\n"
      <<"Licenses may also come from DYNAMSOFT_LICENSE_KEY and SCANDIT_LICENSE_KEY.\n";
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
