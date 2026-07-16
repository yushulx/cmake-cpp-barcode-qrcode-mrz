#include "test_support.h"
#include "barber_dataset.h"
#include <filesystem>
#include <fstream>

using namespace bench;
namespace fs=std::filesystem;

void testBarberParser()
{
    const auto root=fs::temp_directory_path()/"barber_benchmark_fixture";
    fs::remove_all(root); fs::create_directories(root/"images"); fs::create_directories(root/"annotations/nested");
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,2,0,0,0,144,119,83,222,0,0,0,12,73,68,65,84,8,215,99,248,207,192,0,0,3,1,1,0,201,254,146,239,0,0,0,0,73,69,78,68,174,66,96,130};
    std::ofstream image(root/"images/sample.png",std::ios::binary); image.write(reinterpret_cast<const char*>(png),sizeof(png)); image.close();
    std::ofstream duplicate(root/"images/duplicate.png",std::ios::binary); duplicate.write(reinterpret_cast<const char*>(png),sizeof(png)); duplicate.close();
    std::ofstream(root/"images/excluded.png",std::ios::binary).write(reinterpret_cast<const char*>(png),sizeof(png));
    std::ofstream(root/"annotations/nested/source.json")<<R"({"_via_img_metadata":{"sample.png1":{"filename":"sample.png","regions":[{"shape_attributes":{"name":"polygon","all_points_x":[0,1,1],"all_points_y":[0,0,1]},"region_attributes":{"Type":"EAN13","PPE":2.5,"String":"0012345678905"}},{"shape_attributes":{"name":"polygon","all_points_x":[0],"all_points_y":[0]},"region_attributes":{"Type":"1D","String":"abc"}}]},"duplicate.png1":{"filename":"duplicate.png","regions":[{"shape_attributes":{"name":"polygon","all_points_x":[0,1,1],"all_points_y":[0,0,1]},"region_attributes":{"Type":"EAN13","PPE":2.5,"String":"0012345678905"}}]},"excluded.png1":{"filename":"excluded.png","regions":[{"shape_attributes":{"name":"polygon","all_points_x":[0,1,1],"all_points_y":[0,0,1]},"region_attributes":{"Type":"1D","PPE":-1,"String":"-1"}}]}}})";
    BarberDataset dataset; const auto summary=dataset.audit(root/"images",root/"annotations",root/"manifests");
    CHECK(summary.annotation_files==1); CHECK(summary.manifest_images==1); CHECK(summary.annotations==4);
    CHECK(summary.eligible_annotations==2); CHECK(summary.missing_payload_or_generic==2);
    CHECK(summary.duplicate_image_records==2); CHECK(summary.excluded_images_without_ground_truth==1);
    CHECK(summary.benchmark_annotations==1);
    const auto manifest=BarberDataset::readManifest(root/"manifests/benchmark_manifest.jsonl");
    CHECK(manifest.size()==1); CHECK(manifest[0].ground_truth.size()==1);
    CHECK(manifest[0].ground_truth[0].text=="0012345678905");
    CHECK(manifest[0].annotation_file=="nested/source.json"); fs::remove_all(root);
}
