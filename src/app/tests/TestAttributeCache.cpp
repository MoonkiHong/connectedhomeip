/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "lib/core/CHIPTLVTags.h"
#include "protocols/interaction_model/Constants.h"
#include "system/SystemPacketBuffer.h"
#include "system/TLVPacketBufferBackingStore.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeCache.h>
#include <app/data-model/DecodableList.h>
#include <app/data-model/Decode.h>
#include <app/tests/AppTestContext.h>
#include <lib/support/UnitTestRegistration.h>
#include <nlunit-test.h>
#include <string.h>
#include <vector>

using TestContext = chip::Test::AppContext;
using namespace chip::app;
using namespace chip;

namespace {

nlTestSuite * gSuite = nullptr;

struct AttributeInstruction
{
    enum AttributeType
    {
        kAttributeA = 0, // int
        kAttributeB = 1, // byte string
        kAttributeC = 2, // struct
        kAttributeD = 3, // list
    };

    enum ValueType
    {
        kData   = 0,
        kStatus = 1
    };

    AttributeType mAttributeType;
    EndpointId mEndpointId;
    ValueType mValueType;
    uint8_t mInstructionId;

    AttributeInstruction() { mInstructionId = sInstructionId++; }

    bool operator<(const AttributeInstruction & instruction) const
    {
        return (mAttributeType < instruction.mAttributeType ||
                (!(mAttributeType < instruction.mAttributeType) && (mEndpointId < instruction.mEndpointId)));
    }

    AttributeInstruction(AttributeType attributeType, EndpointId endpointId, ValueType valueType) : AttributeInstruction()
    {
        mAttributeType = attributeType;
        mEndpointId    = endpointId;
        mValueType     = valueType;
    }

    AttributeId GetAttributeId() const
    {
        switch (mAttributeType)
        {
        case kAttributeA:
            return Clusters::TestCluster::Attributes::Int16u::Id;
            break;

        case kAttributeB:
            return Clusters::TestCluster::Attributes::OctetString::Id;
            break;

        case kAttributeC:
            return Clusters::TestCluster::Attributes::StructAttr::Id;
            break;

        default:
            return Clusters::TestCluster::Attributes::ListStructOctetString::Id;
            break;
        }
    }

    static uint8_t sInstructionId;
};

uint8_t AttributeInstruction::sInstructionId = 0;

using AttributeInstructionListType = std::vector<AttributeInstruction>;

class DataSeriesGenerator
{
public:
    DataSeriesGenerator(ReadClient::Callback * readCallback, AttributeInstructionListType & instructionList) :
        mReadCallback(readCallback), mInstructionList(instructionList)
    {}

    void Generate();

private:
    ReadClient::Callback * mReadCallback;
    AttributeInstructionListType & mInstructionList;
};

void DataSeriesGenerator::Generate()
{
    System::PacketBufferHandle handle;
    System::PacketBufferTLVWriter writer;
    System::PacketBufferTLVReader reader;
    ReadClient::Callback * callback = mReadCallback;
    StatusIB status;

    callback->OnReportBegin(nullptr);

    uint8_t index = 0;
    for (auto & instruction : mInstructionList)
    {
        ConcreteDataAttributePath path(instruction.mEndpointId, Clusters::TestCluster::Id, 0);
        handle = System::PacketBufferHandle::New(1000);
        writer.Init(std::move(handle), true);
        status            = StatusIB();
        path.mAttributeId = instruction.GetAttributeId();

        ChipLogProgress(DataManagement, "\t -- Generating Instruction ID %d", instruction.mInstructionId);

        if (instruction.mValueType == AttributeInstruction::kData)
        {
            switch (instruction.mAttributeType)
            {
            case AttributeInstruction::kAttributeA: {
                ChipLogProgress(DataManagement, "\t -- Generating A");

                Clusters::TestCluster::Attributes::Int16u::TypeInfo::Type value = instruction.mInstructionId;
                NL_TEST_ASSERT(gSuite, DataModel::Encode(writer, TLV::AnonymousTag(), value) == CHIP_NO_ERROR);
                break;
            }

            case AttributeInstruction::kAttributeB: {
                ChipLogProgress(DataManagement, "\t -- Generating B");

                Clusters::TestCluster::Attributes::OctetString::TypeInfo::Type value;
                uint8_t buf[] = { 'h', 'e', 'l', 'l', 'o' };
                value         = buf;

                NL_TEST_ASSERT(gSuite, DataModel::Encode(writer, TLV::AnonymousTag(), value) == CHIP_NO_ERROR);
                break;
            }

            case AttributeInstruction::kAttributeC: {
                ChipLogProgress(DataManagement, "\t -- Generating C");

                Clusters::TestCluster::Attributes::StructAttr::TypeInfo::Type value;
                value.a = instruction.mInstructionId;
                value.b = true;
                NL_TEST_ASSERT(gSuite, DataModel::Encode(writer, TLV::AnonymousTag(), value) == CHIP_NO_ERROR);
                break;
            }

            case AttributeInstruction::kAttributeD: {
                ChipLogProgress(DataManagement, "\t -- Generating D");

                Clusters::TestCluster::Structs::TestListStructOctet::Type buf[4];

                for (auto & i : buf)
                {
                    i.fabricIndex = instruction.mInstructionId;
                }

                Clusters::TestCluster::Attributes::ListStructOctetString::TypeInfo::Type value;
                path.mListOp = ConcreteDataAttributePath::ListOperation::ReplaceAll;

                value = buf;
                NL_TEST_ASSERT(gSuite, DataModel::Encode(writer, TLV::AnonymousTag(), value) == CHIP_NO_ERROR);
                break;
            }

            default:
                break;
            }

            writer.Finalize(&handle);
            reader.Init(std::move(handle));
            NL_TEST_ASSERT(gSuite, reader.Next() == CHIP_NO_ERROR);
            callback->OnAttributeData(nullptr, path, &reader, status);
        }
        else
        {
            ChipLogProgress(DataManagement, "\t -- Generating Status");
            status.mStatus = Protocols::InteractionModel::Status::Failure;
            callback->OnAttributeData(nullptr, path, nullptr, status);
        }

        index++;
    }

    callback->OnReportEnd(nullptr);
}

class CacheValidator : public AttributeCache::Callback
{
public:
    CacheValidator(AttributeInstructionListType & instructionList);

    Clusters::TestCluster::Attributes::TypeInfo::DecodableType clusterValue;

private:
    void OnDone(ReadClient * apReadClient) override {}
    void DecodeAttribute(const AttributeInstruction & instruction, const ConcreteAttributePath & path, AttributeCache * cache)
    {
        CHIP_ERROR err;
        bool gotStatus = false;

        ChipLogProgress(DataManagement, "\t\t -- Validating Instruction ID: %d", instruction.mInstructionId);

        switch (instruction.mAttributeType)
        {
        case AttributeInstruction::kAttributeA: {
            ChipLogProgress(DataManagement, "\t\t -- Validating A");

            Clusters::TestCluster::Attributes::Int16u::TypeInfo::DecodableType v;
            err = cache->Get<Clusters::TestCluster::Attributes::Int16u::TypeInfo>(path, v);
            if (err == CHIP_ERROR_IM_STATUS_CODE_RECEIVED)
            {
                gotStatus = true;
                err       = CHIP_NO_ERROR;
            }
            else
            {
                NL_TEST_ASSERT(gSuite, err == CHIP_NO_ERROR);
                NL_TEST_ASSERT(gSuite, v == instruction.mInstructionId);
            }

            break;
        }

        case AttributeInstruction::kAttributeB: {
            ChipLogProgress(DataManagement, "\t\t -- Validating B");

            Clusters::TestCluster::Attributes::OctetString::TypeInfo::DecodableType v;
            err = cache->Get<Clusters::TestCluster::Attributes::OctetString::TypeInfo>(path, v);
            if (err == CHIP_ERROR_IM_STATUS_CODE_RECEIVED)
            {
                gotStatus = true;
                err       = CHIP_NO_ERROR;
            }
            else
            {
                NL_TEST_ASSERT(gSuite, err == CHIP_NO_ERROR);
                NL_TEST_ASSERT(gSuite, strncmp((char *) v.data(), "hello", v.size()) == 0);
            }

            break;
        }

        case AttributeInstruction::kAttributeC: {
            ChipLogProgress(DataManagement, "\t\t -- Validating C");

            Clusters::TestCluster::Attributes::StructAttr::TypeInfo::DecodableType v;
            err = cache->Get<Clusters::TestCluster::Attributes::StructAttr::TypeInfo>(path, v);
            if (err == CHIP_ERROR_IM_STATUS_CODE_RECEIVED)
            {
                gotStatus = true;
                err       = CHIP_NO_ERROR;
            }
            else
            {
                NL_TEST_ASSERT(gSuite, v.a == instruction.mInstructionId);
                NL_TEST_ASSERT(gSuite, v.b == true);
            }

            break;
        }

        case AttributeInstruction::kAttributeD: {
            ChipLogProgress(DataManagement, "\t\t -- Validating D");

            Clusters::TestCluster::Attributes::ListStructOctetString::TypeInfo::DecodableType v;
            err = cache->Get<Clusters::TestCluster::Attributes::ListStructOctetString::TypeInfo>(path, v);
            if (err == CHIP_ERROR_IM_STATUS_CODE_RECEIVED)
            {
                gotStatus = true;
                err       = CHIP_NO_ERROR;
            }
            else
            {
                auto listIter = v.begin();
                while (listIter.Next())
                {
                    NL_TEST_ASSERT(gSuite, listIter.GetValue().fabricIndex == instruction.mInstructionId);
                }

                NL_TEST_ASSERT(gSuite, listIter.GetStatus() == CHIP_NO_ERROR);
            }

            break;
        }
        }

        NL_TEST_ASSERT(gSuite, err == CHIP_NO_ERROR);

        if (gotStatus)
        {
            ChipLogProgress(DataManagement, "\t\t -- Validating status");
            NL_TEST_ASSERT(gSuite, instruction.mValueType == AttributeInstruction::kStatus);
        }
    }

    void DecodeClusterObject(const AttributeInstruction & instruction, const ConcreteAttributePath & path, AttributeCache * cache)
    {
        std::list<AttributeCache::AttributeStatus> statusList;
        NL_TEST_ASSERT(gSuite, cache->Get(path.mEndpointId, path.mClusterId, clusterValue, statusList) == CHIP_NO_ERROR);

        if (instruction.mValueType == AttributeInstruction::kData)
        {
            NL_TEST_ASSERT(gSuite, statusList.size() == 0);

            switch (instruction.mAttributeType)
            {
            case AttributeInstruction::kAttributeA:
                ChipLogProgress(DataManagement, "\t\t -- Validating A (Cluster Obj)");
                NL_TEST_ASSERT(gSuite, clusterValue.int16u == instruction.mInstructionId);
                break;

            case AttributeInstruction::kAttributeB:
                ChipLogProgress(DataManagement, "\t\t -- Validating B (Cluster Obj)");
                NL_TEST_ASSERT(gSuite,
                               strncmp((char *) clusterValue.octetString.data(), "hello", clusterValue.octetString.size()) == 0);
                break;

            case AttributeInstruction::kAttributeC:
                ChipLogProgress(DataManagement, "\t\t -- Validating C (Cluster Obj)");
                NL_TEST_ASSERT(gSuite, clusterValue.structAttr.a == instruction.mInstructionId);
                NL_TEST_ASSERT(gSuite, clusterValue.structAttr.b == true);
                break;

            case AttributeInstruction::kAttributeD:
                ChipLogProgress(DataManagement, "\t\t -- Validating D (Cluster Obj)");

                auto listIter = clusterValue.listStructOctetString.begin();
                while (listIter.Next())
                {
                    NL_TEST_ASSERT(gSuite, listIter.GetValue().fabricIndex == instruction.mInstructionId);
                }

                NL_TEST_ASSERT(gSuite, listIter.GetStatus() == CHIP_NO_ERROR);
                break;
            }
        }
        else
        {
            NL_TEST_ASSERT(gSuite, statusList.size() == 1);

            auto status = statusList.front();
            NL_TEST_ASSERT(gSuite, status.mPath.mEndpointId == instruction.mEndpointId);
            NL_TEST_ASSERT(gSuite, status.mPath.mClusterId == Clusters::TestCluster::Id);
            NL_TEST_ASSERT(gSuite, status.mPath.mAttributeId == instruction.GetAttributeId());
            NL_TEST_ASSERT(gSuite, status.mStatus.mStatus == Protocols::InteractionModel::Status::Failure);
        }
    }

    void OnAttributeChanged(AttributeCache * cache, const ConcreteAttributePath & path) override
    {
        StatusIB status;

        // Ensure that the provided path is one that we're expecting to find
        auto iter = mExpectedAttributes.find(path);
        NL_TEST_ASSERT(gSuite, iter != mExpectedAttributes.end());

        // Once retrieved, let's erase it from the expected set so that we can catch duplicates coming back
        // as well as validating that we've seen all attributes at the end.
        mExpectedAttributes.erase(iter);

        for (auto & instruction : mInstructionSet)
        {
            if (instruction.mEndpointId == path.mEndpointId && instruction.GetAttributeId() == path.mAttributeId &&
                path.mClusterId == Clusters::TestCluster::Id)
            {

                //
                // Validate both decoding into attribute objects as well as
                // cluster objects.
                //
                DecodeAttribute(instruction, path, cache);
                DecodeClusterObject(instruction, path, cache);
            }
        }
    }

    void OnClusterChanged(AttributeCache * cache, EndpointId endpointId, ClusterId clusterId) override
    {
        auto iter = mExpectedClusters.find(std::make_tuple(endpointId, clusterId));
        NL_TEST_ASSERT(gSuite, iter != mExpectedClusters.end());
        mExpectedClusters.erase(iter);
    }

    void OnEndpointAdded(AttributeCache * cache, EndpointId endpointId) override
    {
        auto iter = mExpectedEndpoints.find(endpointId);
        NL_TEST_ASSERT(gSuite, iter != mExpectedEndpoints.end());
        mExpectedEndpoints.erase(iter);
    }

    void OnReportEnd(const ReadClient * apReadClient) override
    {
        NL_TEST_ASSERT(gSuite, mExpectedAttributes.size() == 0);
        NL_TEST_ASSERT(gSuite, mExpectedClusters.size() == 0);
        NL_TEST_ASSERT(gSuite, mExpectedEndpoints.size() == 0);
    }

    //
    // We use sets for tracking most of the expected data since we're expecting
    // unique data items being provided in the callbacks.
    //
    std::set<AttributeInstruction> mInstructionSet;
    std::set<ConcreteAttributePath> mExpectedAttributes;
    std::set<std::tuple<EndpointId, ClusterId>> mExpectedClusters;
    std::set<EndpointId> mExpectedEndpoints;
};

CacheValidator::CacheValidator(AttributeInstructionListType & instructionList)
{
    for (auto & instruction : instructionList)
    {
        //
        // We need to replace a matching instruction with the latest one we see in the list to ensure we get
        // the instruction with the highest InstructionID. Hence the erase and insert (i.e replace) operation.
        //
        mInstructionSet.erase(instruction);
        mInstructionSet.insert(instruction);
        mExpectedAttributes.insert(
            ConcreteAttributePath(instruction.mEndpointId, Clusters::TestCluster::Id, instruction.GetAttributeId()));
        mExpectedClusters.insert(std::make_tuple(instruction.mEndpointId, Clusters::TestCluster::Id));
        mExpectedEndpoints.insert(instruction.mEndpointId);
    }
}

void RunAndValidateSequence(AttributeInstructionListType list)
{
    CacheValidator client(list);
    AttributeCache cache(client);
    DataSeriesGenerator generator(&cache.GetBufferedCallback(), list);
    generator.Generate();
}

/*
 * This validates the cache by issuing different sequences of attribute combinations
 * and ensuring that the latest view in the cache matches up with expectations.
 *
 * The print statements indicate the expected output.
 *
 * The legend is as follows:
 *
 * E1:A1 --- Endpoint 1, Attribute A, Version 1
 *
 */
void TestCache(nlTestSuite * apSuite, void * apContext)
{
    ChipLogProgress(DataManagement, "Validating various sequences of attribute data IBs...");

    //
    // Validate a range of types and ensure that they can be successfully decoded.
    //
    ChipLogProgress(DataManagement, "E1:A1 --> E1:A1");
    RunAndValidateSequence({ AttributeInstruction(

        AttributeInstruction::kAttributeA, 1, AttributeInstruction::kData) });

    ChipLogProgress(DataManagement, "E1:B1 --> E1:B1");
    RunAndValidateSequence({ AttributeInstruction(

        AttributeInstruction::kAttributeB, 1, AttributeInstruction::kData) });

    ChipLogProgress(DataManagement, "E1:C1 --> E1:C1");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeC, 1, AttributeInstruction::kData) });

    ChipLogProgress(DataManagement, "E1:D1 --> E1:D1");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData) });

    //
    // Validate that a newer version of a data item over-rides the
    // previous copy.
    //
    ChipLogProgress(DataManagement, "E1:D1 E1:D2 --> E1:D2");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData) });

    //
    // Validate that a newer StatusIB over-rides a previous data value.
    //
    ChipLogProgress(DataManagement, "E1:D1 E1:D2s --> E1:D2s");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kStatus) });

    //
    // Validate that a newer data value over-rides a previous status value.
    //
    ChipLogProgress(DataManagement, "E1:D1s E1:D2 --> E1:D2");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kStatus),
                             AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData) });

    //
    // Validate data across different endpoints.
    //
    ChipLogProgress(DataManagement, "E0:D1 E1:D2 --> E0:D1 E1:D2");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeD, 0, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeD, 1, AttributeInstruction::kData) });

    ChipLogProgress(DataManagement, "E0:A1 E0:B2 E0:A3 E0:B4 --> E0:A3 E0:B4");
    RunAndValidateSequence({ AttributeInstruction(AttributeInstruction::kAttributeA, 0, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeB, 0, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeA, 0, AttributeInstruction::kData),
                             AttributeInstruction(AttributeInstruction::kAttributeB, 0, AttributeInstruction::kData) });
}

// clang-format off
const nlTest sTests[] =
{
    NL_TEST_DEF("TestCache", TestCache),
    NL_TEST_SENTINEL()
};

nlTestSuite theSuite =
{
    "TestAttributeCache",
    &sTests[0],
    TestContext::Initialize,
    TestContext::Finalize
};

}
// clang-format on

int TestAttributeCache()
{
    TestContext gContext;
    gSuite = &theSuite;
    nlTestRunner(&theSuite, &gContext);
    return (nlTestRunnerStats(&theSuite));
}

CHIP_REGISTER_TEST_SUITE(TestAttributeCache)
