#include "test_support.h"
#include "matcher.h"
#include "normalization.h"

using namespace bench;

void testMatching()
{
    CHECK(canonicalFormat("EAN13") == "EAN_13");
    CHECK(canonicalFormat("QR Code") == "QR_CODE");
    CHECK(canonicalFormat("CODE39EXTENDED") == "CODE_39");
    CHECK(normalizedPayload("UPC_A","0012345678905") == "012345678905");
    CHECK(isPayloadStructurallyValid("EAN_13","0012345678905"));
    CHECK(!isPayloadStructurallyValid("EAN_13","0012345678904"));
    GroundTruth a{"a","EAN_13","0012345678905",{},std::nullopt,true,{}};
    GroundTruth b=a; b.annotation_id="b";
    DecodedBarcode prediction{"EAN_13",{},"0012345678905",std::nullopt};
    auto matches=matchResults({a,b},{prediction},"zxing-cpp");
    CHECK(matches.size()==2);
    CHECK(matches[0].outcome==Outcome::Correct);
    CHECK(matches[1].outcome==Outcome::NotFound);
    GroundTruth postal{"p","POSTNET","12345",{},std::nullopt,true,{}};
    matches=matchResults({postal},{},"zxing-cpp");
    CHECK(matches[0].outcome==Outcome::UnsupportedFormat);
    a.polygon={{0,0},{1,1}};
    matches=matchResults({a},{prediction},"zxing-cpp");
    CHECK(matches[0].outcome==Outcome::Correct);
    GroundTruth code39{"c39","CODE_39","ABC123",{},std::nullopt,true,{}};
    DecodedBarcode code39_extended{"CODE39EXTENDED",{},"ABC123",std::nullopt};
    matches=matchResults({code39},{code39_extended},"dynamsoft-dbr");
    CHECK(matches[0].outcome==Outcome::Correct);

    CHECK(normalizedPayload("CODE_39","*8974589*") == "8974589");
    CHECK(normalizedPayload("CODE_128","{GS}8952180") == "8952180");
    CHECK(normalizedPayload("QR_CODE","t=1&amp;s=2\n") == "t=1&s=2");
    CHECK(normalizedPayload("QR_CODE","\\000001https://example") == "https://example");
    CHECK(isUnreliablePlaceholder("^"));
    CHECK(!isUnreliablePlaceholder("12"));

    GroundTruth starred{"star","CODE_39","*8974589*",{},std::nullopt,true,{}};
    DecodedBarcode code39_payload{"CODE_39",{},"8974589",std::nullopt};
    matches=matchResults({starred},{code39_payload},"zxing-cpp");
    CHECK(matches.size()==1);
    CHECK(matches[0].outcome==Outcome::Correct);

    GroundTruth gs1{"gs","CODE_128","8952180",{},std::nullopt,true,{}};
    DecodedBarcode gs_pred{"CODE_128",{},"{GS}8952180",std::nullopt};
    matches=matchResults({gs1},{gs_pred},"dynamsoft-dbr");
    CHECK(matches[0].outcome==Outcome::Correct);

    GroundTruth html_gt{"html","QR_CODE","a&amp;b",{},std::nullopt,true,{}};
    DecodedBarcode html_pred{"QR_CODE",{},"a&b",std::nullopt};
    matches=matchResults({html_gt},{html_pred},"zxing-cpp");
    CHECK(matches[0].outcome==Outcome::Correct);

    GroundTruth placeholder{"ph","PDF_417","^",{},std::nullopt,true,{}};
    DecodedBarcode placeholder_pred{"PDF_417",{},"M1FORTIN",std::nullopt};
    matches=matchResults({placeholder},{placeholder_pred},"dynamsoft-dbr");
    CHECK(matches.size()==1);
    CHECK(matches[0].outcome==Outcome::ExtraResult);
}
