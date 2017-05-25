/**
 * Copyright (C) 2017 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/db/update/update_object_node.h"

#include "mongo/bson/mutable/algorithm.h"
#include "mongo/bson/mutable/mutable_bson_test_utils.h"
#include "mongo/db/json.h"
#include "mongo/db/query/collation/collator_interface_mock.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
namespace {

using mongo::mutablebson::Document;
using mongo::mutablebson::Element;

TEST(UpdateObjectNodeTest, InvalidPathFailsToParse) {
    auto update = fromjson("{$set: {'': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"][""], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::EmptyFieldName);
    ASSERT_EQ(result.getStatus().reason(), "An empty update path is not valid.");
}

TEST(UpdateObjectNodeTest, ValidPathParsesSuccessfully) {
    auto update = fromjson("{$set: {'a.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator));
}

TEST(UpdateObjectNodeTest, MultiplePositionalElementsFailToParse) {
    auto update = fromjson("{$set: {'a.$.b.$': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b.$"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::BadValue);
    ASSERT_EQ(result.getStatus().reason(),
              "Too many positional (i.e. '$') elements found in path 'a.$.b.$'");
}

TEST(UpdateObjectNodeTest, ParsingSetsPositionalTrue) {
    auto update = fromjson("{$set: {'a.$.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b"], collator);
    ASSERT_OK(result);
    ASSERT_TRUE(result.getValue());
}

TEST(UpdateObjectNodeTest, ParsingSetsPositionalFalse) {
    auto update = fromjson("{$set: {'a.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator);
    ASSERT_OK(result);
    ASSERT_FALSE(result.getValue());
}

TEST(UpdateObjectNodeTest, PositionalElementFirstPositionFailsToParse) {
    auto update = fromjson("{$set: {'$': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["$"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::BadValue);
    ASSERT_EQ(result.getStatus().reason(),
              "Cannot have positional (i.e. '$') element in the first position in path '$'");
}

// TODO SERVER-28777: All modifier types should succeed.
TEST(UpdateObjectNodeTest, IncFailsToParse) {
    auto update = fromjson("{$inc: {a: 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_INC, update["$inc"]["a"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::FailedToParse);
    ASSERT_EQ(result.getStatus().reason(), "Cannot construct modifier of type 3");
}

TEST(UpdateObjectNodeTest, TwoModifiersOnSameFieldFailToParse) {
    auto update = fromjson("{$set: {a: 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(), "Updating the path 'a' would create a conflict at 'a'");
}

TEST(UpdateObjectNodeTest, TwoModifiersOnDifferentFieldsParseSuccessfully) {
    auto update = fromjson("{$set: {a: 5, b: 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["b"], collator));
}

TEST(UpdateObjectNodeTest, TwoModifiersWithSameDottedPathFailToParse) {
    auto update = fromjson("{$set: {'a.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.b' would create a conflict at 'a.b'");
}

TEST(UpdateObjectNodeTest, FirstModifierPrefixOfSecondFailToParse) {
    auto update = fromjson("{$set: {a: 5, 'a.b': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.b' would create a conflict at 'a'");
}

TEST(UpdateObjectNodeTest, FirstModifierDottedPrefixOfSecondFailsToParse) {
    auto update = fromjson("{$set: {'a.b': 5, 'a.b.c': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b.c"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.b.c' would create a conflict at 'a.b'");
}

TEST(UpdateObjectNodeTest, SecondModifierPrefixOfFirstFailsToParse) {
    auto update = fromjson("{$set: {'a.b': 5, a: 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(), "Updating the path 'a' would create a conflict at 'a'");
}

TEST(UpdateObjectNodeTest, SecondModifierDottedPrefixOfFirstFailsToParse) {
    auto update = fromjson("{$set: {'a.b.c': 5, 'a.b': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b.c"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.b' would create a conflict at 'a.b'");
}

TEST(UpdateObjectNodeTest, ModifiersWithCommonPrefixParseSuccessfully) {
    auto update = fromjson("{$set: {'a.b': 5, 'a.c': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.c"], collator));
}

TEST(UpdateObjectNodeTest, ModifiersWithCommonDottedPrefixParseSuccessfully) {
    auto update = fromjson("{$set: {'a.b.c': 5, 'a.b.d': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b.d"], collator));
}

TEST(UpdateObjectNodeTest, ModifiersWithCommonPrefixDottedSuffixParseSuccessfully) {
    auto update = fromjson("{$set: {'a.b.c': 5, 'a.d.e': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.b.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.d.e"], collator));
}

TEST(UpdateObjectNodeTest, TwoModifiersOnSamePositionalFieldFailToParse) {
    auto update = fromjson("{$set: {'a.$': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.$' would create a conflict at 'a.$'");
}

TEST(UpdateObjectNodeTest, PositionalFieldsWithDifferentPrefixesParseSuccessfully) {
    auto update = fromjson("{$set: {'a.$': 5, 'b.$': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["b.$"], collator));
}

TEST(UpdateObjectNodeTest, PositionalAndNonpositionalFieldWithCommonPrefixParseSuccessfully) {
    auto update = fromjson("{$set: {'a.$': 5, 'a.0': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.0"], collator));
}

TEST(UpdateObjectNodeTest, TwoModifiersWithSamePositionalDottedPathFailToParse) {
    auto update = fromjson("{$set: {'a.$.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.$.b' would create a conflict at 'a.$.b'");
}

TEST(UpdateObjectNodeTest, FirstModifierPositionalPrefixOfSecondFailsToParse) {
    auto update = fromjson("{$set: {'a.$': 5, 'a.$.b': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.$.b' would create a conflict at 'a.$'");
}

TEST(UpdateObjectNodeTest, SecondModifierPositionalPrefixOfFirstFailsToParse) {
    auto update = fromjson("{$set: {'a.$.b': 5, 'a.$': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$.b"], collator));
    auto result = UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a.$"], collator);
    ASSERT_NOT_OK(result);
    ASSERT_EQ(result.getStatus().code(), ErrorCodes::ConflictingUpdateOperators);
    ASSERT_EQ(result.getStatus().reason(),
              "Updating the path 'a.$' would create a conflict at 'a.$'");
}

TEST(UpdateObjectNodeTest, FirstModifierFieldPrefixOfSecondParsesSuccessfully) {
    auto update = fromjson("{$set: {'a': 5, 'ab': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["ab"], collator));
}

TEST(UpdateObjectNodeTest, SecondModifierFieldPrefixOfSecondParsesSuccessfully) {
    auto update = fromjson("{$set: {'ab': 5, 'a': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["ab"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, update["$set"]["a"], collator));
}

/**
 * Used to test if the fields in an input UpdateObjectNode match an expected set of fields.
 */
static bool fieldsMatch(const std::vector<std::string>& expectedFields,
                        const UpdateObjectNode& node) {
    for (const std::string& fieldName : expectedFields) {
        if (!node.getChild(fieldName)) {
            return false;
        }
    }

    // There are no expected fields that aren't in the UpdateObjectNode. There is no way to check
    // if UpdateObjectNodes contains any fields that are not in the expected set, because the
    // UpdateObjectNodes API does not expose its list of child fields in any way other than
    // getChild().
    return true;
}

TEST(UpdateObjectNodeTest, DistinctFieldsMergeCorrectly) {
    auto setUpdate1 = fromjson("{$set: {'a': 5}}");
    auto setUpdate2 = fromjson("{$set: {'ab': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["ab"], collator));

    auto result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*result) == typeid(UpdateObjectNode&));
    auto mergedRootNode = static_cast<UpdateObjectNode*>(result.get());
    ASSERT_TRUE(fieldsMatch(std::vector<std::string>{"a", "ab"}, *mergedRootNode));
}

TEST(UpdateObjectNodeTest, NestedMergeSucceeds) {
    auto setUpdate1 = fromjson("{$set: {'a.c': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.d': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.d"], collator));

    auto result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*result) == typeid(UpdateObjectNode&));
    auto mergedRootNode = static_cast<UpdateObjectNode*>(result.get());
    ASSERT_TRUE(fieldsMatch({"a"}, *mergedRootNode));

    ASSERT_TRUE(mergedRootNode->getChild("a"));
    ASSERT_TRUE(mergedRootNode->getChild("a")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*mergedRootNode->getChild("a")) == typeid(UpdateObjectNode&));
    auto aNode = static_cast<UpdateObjectNode*>(mergedRootNode->getChild("a"));
    ASSERT_TRUE(fieldsMatch({"c", "d"}, *aNode));
}

TEST(UpdateObjectNodeTest, DoublyNestedMergeSucceeds) {
    auto setUpdate1 = fromjson("{$set: {'a.b.c': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.b.d': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.b.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.b.d"], collator));

    auto result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*result) == typeid(UpdateObjectNode&));
    auto mergedRootNode = static_cast<UpdateObjectNode*>(result.get());
    ASSERT_TRUE(fieldsMatch({"a"}, *mergedRootNode));

    ASSERT_TRUE(mergedRootNode->getChild("a"));
    ASSERT_TRUE(mergedRootNode->getChild("a")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*mergedRootNode->getChild("a")) == typeid(UpdateObjectNode&));
    auto aNode = static_cast<UpdateObjectNode*>(mergedRootNode->getChild("a"));
    ASSERT_TRUE(fieldsMatch({"b"}, *aNode));

    ASSERT_TRUE(aNode->getChild("b"));
    ASSERT_TRUE(aNode->getChild("b")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*aNode->getChild("b")) == typeid(UpdateObjectNode&));
    auto bNode = static_cast<UpdateObjectNode*>(aNode->getChild("b"));
    ASSERT_TRUE(fieldsMatch({"c", "d"}, *bNode));
}

TEST(UpdateObjectNodeTest, FieldAndPositionalMergeCorrectly) {
    auto setUpdate1 = fromjson("{$set: {'a.b': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.$': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.$"], collator));

    auto result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*result) == typeid(UpdateObjectNode&));
    auto mergedRootNode = static_cast<UpdateObjectNode*>(result.get());
    ASSERT_TRUE(fieldsMatch(std::vector<std::string>{"a"}, *mergedRootNode));

    ASSERT_TRUE(mergedRootNode->getChild("a"));
    ASSERT_TRUE(mergedRootNode->getChild("a")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*mergedRootNode->getChild("a")) == typeid(UpdateObjectNode&));
    auto aNode = static_cast<UpdateObjectNode*>(mergedRootNode->getChild("a"));
    ASSERT_TRUE(aNode->getChild("$"));
    ASSERT_TRUE(fieldsMatch({"b"}, *aNode));
}

TEST(UpdateObjectNodeTest, MergeThroughPositionalSucceeds) {
    auto setUpdate1 = fromjson("{$set: {'a.$.b': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.$.c': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.$.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.$.c"], collator));

    auto result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*result) == typeid(UpdateObjectNode&));
    auto mergedRootNode = static_cast<UpdateObjectNode*>(result.get());
    ASSERT_TRUE(fieldsMatch({"a"}, *mergedRootNode));

    ASSERT_TRUE(mergedRootNode->getChild("a"));
    ASSERT_TRUE(mergedRootNode->getChild("a")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*mergedRootNode->getChild("a")) == typeid(UpdateObjectNode&));
    auto aNode = static_cast<UpdateObjectNode*>(mergedRootNode->getChild("a"));
    ASSERT_TRUE(fieldsMatch({}, *aNode));

    ASSERT_TRUE(aNode->getChild("$"));
    ASSERT_TRUE(aNode->getChild("$")->type == UpdateNode::Type::Object);
    ASSERT_TRUE(typeid(*aNode->getChild("$")) == typeid(UpdateObjectNode&));
    auto positionalNode = static_cast<UpdateObjectNode*>(aNode->getChild("$"));
    ASSERT_TRUE(fieldsMatch({"b", "c"}, *positionalNode));
}

TEST(UpdateObjectNodeTest, TopLevelConflictFails) {
    auto setUpdate1 = fromjson("{$set: {'a': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a'");
}

TEST(UpdateObjectNodeTest, NestedConflictFails) {
    auto setUpdate1 = fromjson("{$set: {'a.b': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.b': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.b"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.b'");
}

TEST(UpdateObjectNodeTest, LeftPrefixMergeFails) {
    auto setUpdate1 = fromjson("{$set: {'a.b': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.b.c': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.b.c"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.b'");
}

TEST(UpdateObjectNodeTest, RightPrefixMergeFails) {
    auto setUpdate1 = fromjson("{$set: {'a.b.c': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.b': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.b.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.b"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.b'");
}

TEST(UpdateObjectNodeTest, LeftPrefixMergeThroughPositionalFails) {
    auto setUpdate1 = fromjson("{$set: {'a.$.c': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.$.c.d': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.$.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.$.c.d"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.$.c'");
}

TEST(UpdateObjectNodeTest, RightPrefixMergeThroughPositionalFails) {
    auto setUpdate1 = fromjson("{$set: {'a.$.c.d': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.$.c': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.$.c.d"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.$.c"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.$.c'");
}

TEST(UpdateObjectNodeTest, MergeWithConflictingPositionalFails) {
    auto setUpdate1 = fromjson("{$set: {'a.$': 5}}");
    auto setUpdate2 = fromjson("{$set: {'a.$': 6}}");
    FieldRef fakeFieldRef("root");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode setRoot1, setRoot2;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot1, modifiertable::ModifierType::MOD_SET, setUpdate1["$set"]["a.$"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &setRoot2, modifiertable::ModifierType::MOD_SET, setUpdate2["$set"]["a.$"], collator));

    std::unique_ptr<UpdateNode> result;
    ASSERT_THROWS_CODE_AND_WHAT(
        result = UpdateNode::createUpdateNodeByMerging(setRoot1, setRoot2, &fakeFieldRef),
        UserException,
        ErrorCodes::ConflictingUpdateOperators,
        "Update created a conflict at 'root.a.$'");
}

TEST(UpdateObjectNodeTest, ApplyCreateField) {
    auto setUpdate = fromjson("{$set: {b: 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));

    Document doc(fromjson("{a: 5}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("b");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_EQUALS(fromjson("{a: 5, b: 6}"), doc);
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_EQUALS(fromjson("{$set: {b: 6}}"), logDoc);
}

TEST(UpdateObjectNodeTest, ApplyExistingField) {
    auto setUpdate = fromjson("{$set: {a: 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a"], collator));

    Document doc(fromjson("{a: 5}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_EQUALS(fromjson("{a: 6}"), doc);
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_EQUALS(fromjson("{$set: {a: 6}}"), logDoc);
}

TEST(UpdateObjectNodeTest, ApplyExistingAndNonexistingFields) {
    auto setUpdate = fromjson("{$set: {a: 5, b: 6, c: 7, d: 8}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["d"], collator));

    Document doc(fromjson("{a: 0, c: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: 5, c: 7, b: 6, d: 8}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {a: 5, b: 6, c: 7, d: 8}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyExistingNestedPaths) {
    auto setUpdate = fromjson("{$set: {'a.b': 6, 'a.c': 7, 'b.d': 8, 'b.e': 9}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b.d"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b.e"], collator));

    Document doc(fromjson("{a: {b: 5, c: 5}, b: {d: 5, e: 5}}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: {b: 6, c: 7}, b: {d: 8, e: 9}}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.b': 6, 'a.c': 7, 'b.d': 8, 'b.e': 9}}"),
                      logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyCreateNestedPaths) {
    auto setUpdate = fromjson("{$set: {'a.b': 6, 'a.c': 7, 'b.d': 8, 'b.e': 9}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b.d"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b.e"], collator));

    Document doc(fromjson("{z: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{z: 0, a: {b: 6, c: 7}, b: {d: 8, e: 9}}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.b': 6, 'a.c': 7, 'b.d': 8, 'b.e': 9}}"),
                      logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyCreateDeeplyNestedPaths) {
    auto setUpdate = fromjson("{$set: {'a.b.c.d': 6, 'a.b.c.e': 7, 'a.f': 8}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b.c.d"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b.c.e"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.f"], collator));

    Document doc(fromjson("{z: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{z: 0, a: {b: {c: {d: 6, e: 7}}, f: 8}}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.b.c.d': 6, 'a.b.c.e': 7, 'a.f': 8}}"),
                      logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ChildrenShouldBeAppliedInAlphabeticalOrder) {
    auto setUpdate = fromjson("{$set: {a: 5, d: 6, c: 7, b: 8, z: 9}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["d"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["z"], collator));

    Document doc(fromjson("{z: 0, a: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{z: 9, a: 5, b: 8, c: 7, d: 6}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {a: 5, b: 8, c: 7, d: 6, z: 9}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, CollatorShouldNotAffectUpdateOrder) {
    auto setUpdate = fromjson("{$set: {abc: 5, cba: 6}}");
    CollatorInterfaceMock collator(CollatorInterfaceMock::MockType::kReverseString);
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["abc"], &collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["cba"], &collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("abc");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{abc: 5, cba: 6}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {abc: 5, cba: 6}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyNoop) {
    auto setUpdate = fromjson("{$set: {a: 5, b: 6, c: 7}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["c"], collator));

    Document doc(fromjson("{a: 5, b: 6, c: 7}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    indexData.addPath("b");
    indexData.addPath("c");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_FALSE(indexesAffected);
    ASSERT_TRUE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: 5, b: 6, c: 7}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplySomeChildrenNoops) {
    auto setUpdate = fromjson("{$set: {a: 5, b: 6, c: 7}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["c"], collator));

    Document doc(fromjson("{a: 5, b: 0, c: 7}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    indexData.addPath("b");
    indexData.addPath("c");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: 5, b: 6, c: 7}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {b: 6}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyBlockingElement) {
    auto setUpdate = fromjson("{$set: {'a.b': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b"], collator));

    Document doc(fromjson("{a: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_THROWS_CODE_AND_WHAT(root.apply(doc.root(),
                                           &pathToCreate,
                                           &pathTaken,
                                           matchedField,
                                           fromReplication,
                                           &indexData,
                                           &logBuilder,
                                           &indexesAffected,
                                           &noop),
                                UserException,
                                ErrorCodes::PathNotViable,
                                "Cannot create field 'b' in element {a: 0}");
}

TEST(UpdateObjectNodeTest, ApplyBlockingElementFromReplication) {
    auto setUpdate = fromjson("{$set: {'a.b': 5, b: 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["b"], collator));

    Document doc(fromjson("{a: 0}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = true;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_FALSE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: 0, b: 6}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {b: 6}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyPositionalMissingMatchedField) {
    auto setUpdate = fromjson("{$set: {'a.$': 5}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$"], collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField;
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_THROWS_CODE_AND_WHAT(
        root.apply(doc.root(),
                   &pathToCreate,
                   &pathTaken,
                   matchedField,
                   fromReplication,
                   &indexData,
                   &logBuilder,
                   &indexesAffected,
                   &noop),
        UserException,
        ErrorCodes::BadValue,
        "The positional operator did not find the match needed from the query.");
}

TEST(UpdateObjectNodeTest, ApplyMergePositionalChild) {
    auto setUpdate = fromjson("{$set: {'a.0.b': 5, 'a.$.c': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$.c"], collator));

    Document doc(fromjson("{a: [{b: 0, c: 0}]}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "0";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: [{b: 5, c: 6}]}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0.b': 5, 'a.0.c': 6}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyOrderMergedPositionalChild) {
    auto setUpdate = fromjson("{$set: {'a.2': 5, 'a.1.b': 6, 'a.0': 7, 'a.$.c': 8}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.2"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.1.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$.c"], collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "1";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: {'0': 7, '1': {b: 6, c: 8}, '2': 5}}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0': 7, 'a.1.b': 6, 'a.1.c': 8, 'a.2': 5}}"),
                      logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyMergeConflictWithPositionalChild) {
    auto setUpdate = fromjson("{$set: {'a.0': 5, 'a.$': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$"], collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "0";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_THROWS_CODE_AND_WHAT(root.apply(doc.root(),
                                           &pathToCreate,
                                           &pathTaken,
                                           matchedField,
                                           fromReplication,
                                           &indexData,
                                           &logBuilder,
                                           &indexesAffected,
                                           &noop),
                                UserException,
                                ErrorCodes::ConflictingUpdateOperators,
                                "Update created a conflict at 'a.0'");
}

TEST(UpdateObjectNodeTest, ApplyDoNotMergePositionalChild) {
    auto setUpdate = fromjson("{$set: {'a.0': 5, 'a.2': 6, 'a.$': 7}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.2"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$"], collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "1";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: {'0': 5, '1': 7, '2': 6}}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0': 5, 'a.1': 7, 'a.2': 6}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyPositionalChildLast) {
    auto setUpdate = fromjson("{$set: {'a.$': 5, 'a.0': 6, 'a.1': 7}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.1"], collator));

    Document doc(fromjson("{}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "2";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: {'0': 6, '1': 7, '2': 5}}"), doc.getObject());
    ASSERT_FALSE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0': 6, 'a.1': 7, 'a.2': 5}}"), logDoc.getObject());
}

TEST(UpdateObjectNodeTest, ApplyUseStoredMergedPositional) {
    auto setUpdate = fromjson("{$set: {'a.0.b': 5, 'a.$.c': 6}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$.c"], collator));

    Document doc(fromjson("{a: [{b: 0, c: 0}]}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "0";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: [{b: 5, c: 6}]}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0.b': 5, 'a.0.c': 6}}"), logDoc.getObject());

    Document doc2(fromjson("{a: [{b: 0, c: 0}]}"));
    Document logDoc2;
    LogBuilder logBuilder2(logDoc2.root());
    ASSERT_OK(root.apply(doc2.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder2,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: [{b: 5, c: 6}]}"), doc2.getObject());
    ASSERT_TRUE(doc2.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0.b': 5, 'a.0.c': 6}}"), logDoc2.getObject());
}

TEST(UpdateObjectNodeTest, ApplyDoNotUseStoredMergedPositional) {
    auto setUpdate = fromjson("{$set: {'a.0.b': 5, 'a.$.c': 6, 'a.1.d': 7}}");
    const CollatorInterface* collator = nullptr;
    UpdateObjectNode root;
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.0.b"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.$.c"], collator));
    ASSERT_OK(UpdateObjectNode::parseAndMerge(
        &root, modifiertable::ModifierType::MOD_SET, setUpdate["$set"]["a.1.d"], collator));

    Document doc(fromjson("{a: [{b: 0, c: 0}, {c: 0, d: 0}]}"));
    FieldRef pathToCreate("");
    FieldRef pathTaken("");
    StringData matchedField = "0";
    auto fromReplication = false;
    UpdateIndexData indexData;
    indexData.addPath("a");
    Document logDoc;
    LogBuilder logBuilder(logDoc.root());
    auto indexesAffected = false;
    auto noop = false;
    ASSERT_OK(root.apply(doc.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField,
                         fromReplication,
                         &indexData,
                         &logBuilder,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: [{b: 5, c: 6}, {c: 0, d: 7}]}"), doc.getObject());
    ASSERT_TRUE(doc.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0.b': 5, 'a.0.c': 6, 'a.1.d': 7}}"), logDoc.getObject());

    Document doc2(fromjson("{a: [{b: 0, c: 0}, {c: 0, d: 0}]}"));
    StringData matchedField2 = "1";
    Document logDoc2;
    LogBuilder logBuilder2(logDoc2.root());
    ASSERT_OK(root.apply(doc2.root(),
                         &pathToCreate,
                         &pathTaken,
                         matchedField2,
                         fromReplication,
                         &indexData,
                         &logBuilder2,
                         &indexesAffected,
                         &noop));
    ASSERT_TRUE(indexesAffected);
    ASSERT_FALSE(noop);
    ASSERT_BSONOBJ_EQ(fromjson("{a: [{b: 5, c: 0}, {c: 6, d: 7}]}"), doc2.getObject());
    ASSERT_TRUE(doc2.isInPlaceModeEnabled());
    ASSERT_BSONOBJ_EQ(fromjson("{$set: {'a.0.b': 5, 'a.1.c': 6, 'a.1.d': 7}}"),
                      logDoc2.getObject());
}

}  // namespace
}  // namespace mongo
