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

#include "mongo/db/repl/idempotency_update_sequence.h"

#include <algorithm>
#include <memory>

#include "mongo/db/field_ref.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/repl/idempotency_document_structure.h"

namespace mongo {

std::size_t UpdateSequenceGenerator::_getPathDepth(const std::string& path) {
    // Our depth is -1 because we count at 0, but numParts just counts the number of fields.
    return path == "" ? 0 : FieldRef(path).numParts() - 1;
}

std::vector<std::string> UpdateSequenceGenerator::_eliminatePrefixPaths(
    const std::string& path, const std::vector<std::string>& paths) {
    std::vector<std::string> remainingPaths;
    for (auto oldPath : paths) {
        if (!FieldRef(oldPath).isPrefixOf(FieldRef(path)) &&
            !FieldRef(path).isPrefixOf(FieldRef(oldPath)) && path != path) {
            remainingPaths.push_back(oldPath);
        }
    }

    return remainingPaths;
}

void UpdateSequenceGenerator::_generatePaths(const std::set<StringData>& fields,
                                             const std::size_t depth,
                                             const std::size_t length,
                                             const std::string& path) {
    if (UpdateSequenceGenerator::_getPathDepth(path) == depth) {
        return;
    }

    if (!path.empty()) {
        for (std::size_t i = 0; i < length; i++) {
            FieldRef arrPathRef(path);
            arrPathRef.appendPart(std::to_string(i));
            auto arrPath = arrPathRef.dottedField().toString();
            _paths.push_back(arrPath);
            _generatePaths(fields, depth, length, arrPath);
        }
    }

    if (fields.empty()) {
        return;
    }

    std::set<StringData> remainingFields(fields);
    for (auto field : fields) {
        remainingFields.erase(remainingFields.begin());
        FieldRef docPathRef(path);
        docPathRef.appendPart(field);
        auto docPath = docPathRef.dottedField().toString();
        _paths.push_back(docPath);
        _generatePaths(remainingFields, depth, length, docPath);
    }
}

std::vector<std::string> UpdateSequenceGenerator::_getRandomPaths() const {
    std::size_t randomAmountOfArgs = this->_random.nextInt32(this->_paths.size()) + 1;
    std::vector<std::string> randomPaths;
    std::vector<std::string> validPaths(this->_paths);

    for (std::size_t i = 0; i < randomAmountOfArgs; i++) {
        int randomIndex = UpdateSequenceGenerator::_random.nextInt32(validPaths.size());
        std::string randomPath = validPaths[randomIndex];
        randomPaths.push_back(randomPath);
        validPaths = UpdateSequenceGenerator::_eliminatePrefixPaths(randomPath, validPaths);
        if (validPaths.empty()) {
            break;
        }
    }

    return randomPaths;
}

BSONObj UpdateSequenceGenerator::generateUpdate() const {
    bool generateSetUpdate =
        this->_random.nextInt32(UpdateSequenceGenerator::kNumUpdateChoices) == 1;
    if (generateSetUpdate) {
        return _generateSet();
    } else {
        return _generateUnset();
    }
}

BSONObj UpdateSequenceGenerator::_generateSet() const {
    BSONObjBuilder setBuilder;
    {
        BSONObjBuilder setArgBuilder(setBuilder.subobjStart("$set"));

        for (auto randomPath : _getRandomPaths()) {
            _appendSetArgToBuilder(randomPath, &setArgBuilder);
        }
    }
    return setBuilder.obj();
}

UpdateSequenceGenerator::SetChoice UpdateSequenceGenerator::_determineWhatToSet(
    const std::string& setPath) const {
    if (UpdateSequenceGenerator::_getPathDepth(setPath) == this->_depth) {
        auto choice = static_cast<size_t>(SetChoice::kNumScalarSetChoices);
        return static_cast<SetChoice>(this->_random.nextInt32(choice));
    } else {
        auto choice = static_cast<size_t>(SetChoice::kNumScalarSetChoices);
        return static_cast<SetChoice>(UpdateSequenceGenerator::_random.nextInt32(choice));
    }
}

void UpdateSequenceGenerator::_appendSetArgToBuilder(const std::string& setPath,
                                                     BSONObjBuilder* setArgBuilder) const {
    auto setChoice = _determineWhatToSet(setPath);
    switch (setChoice) {
        case SetChoice::kSetNumeric:
            setArgBuilder->append(setPath, _generateNumericToSet());
            return;
        case SetChoice::kSetNull:
            setArgBuilder->appendNull(setPath);
            return;
        case SetChoice::kSetBool:
            setArgBuilder->append(setPath, _generateBoolToSet());
            return;
        case SetChoice::kSetArr:
            setArgBuilder->append(setPath, _generateArrToSet(setPath));
            return;
        case SetChoice::kSetDoc:
            setArgBuilder->append(setPath, _generateDocToSet(setPath));
            return;
        case SetChoice::kNumTotalSetChoices:
            MONGO_UNREACHABLE;
    }
    MONGO_UNREACHABLE;
}

BSONObj UpdateSequenceGenerator::_generateUnset() const {
    BSONObjBuilder unsetBuilder;
    {
        BSONObjBuilder unsetArgBuilder(unsetBuilder.subobjStart("$unset"));

        for (auto randomPath : _getRandomPaths()) {
            unsetArgBuilder.appendNull(randomPath);
        }
    }

    return unsetBuilder.obj();
}

double UpdateSequenceGenerator::_generateNumericToSet() const {
    return UpdateSequenceGenerator::_random.nextCanonicalDouble() * INT_MAX;
}

bool UpdateSequenceGenerator::_generateBoolToSet() const {
    return this->_random.nextInt32(2) == 1;
}

BSONArray UpdateSequenceGenerator::_generateArrToSet(const std::string& setPath) const {
    auto enumerator = _getValidEnumeratorForPath(setPath);

    auto possibleArrs = enumerator.enumerateArrs();
    std::size_t randomIndex = this->_random.nextInt32(possibleArrs.size());
    auto chosenArr = possibleArrs[randomIndex];

    return chosenArr;
}

BSONObj UpdateSequenceGenerator::_generateDocToSet(const std::string& setPath) const {
    auto enumerator = _getValidEnumeratorForPath(setPath);
    std::size_t randomIndex = this->_random.nextInt32(enumerator.getDocs().size());
    return enumerator.getDocs()[randomIndex];
}

std::set<StringData> UpdateSequenceGenerator::_getRemainingFields(const std::string& path) const {
    std::set<StringData> remainingFields(this->_fields);

    FieldRef pathRef(path);
    StringData lastField;
    // This is guaranteed to terminate with a value for lastField, since no valid path contains only
    // array positions (numbers).
    for (int i = pathRef.numParts() - 1; i >= 0; i--) {
        auto field = pathRef.getPart(i);
        if (this->_fields.find(field) != this->_fields.end()) {
            lastField = field;
            break;
        }
    }

    // The last alphabetic field used must be after all other alphabetic fields that could ever be
    // used, since the fields that are used are selected in the order that they pop off from a
    // std::set.
    for (auto field : this->_fields) {
        remainingFields.erase(field);
        if (field == lastField) {
            break;
        }
    }

    return remainingFields;
}

DocumentStructureEnumerator UpdateSequenceGenerator::_getValidEnumeratorForPath(
    const std::string& path) const {
    auto remainingFields = _getRemainingFields(path);
    std::size_t remainingDepth = this->_depth - UpdateSequenceGenerator::_getPathDepth(path);
    if (remainingDepth > 0) {
        remainingDepth -= 1;
    }

    DocumentStructureEnumerator enumerator({remainingFields, remainingDepth, this->_length});
    return enumerator;
}

BSONObj UpdateSequenceGenerator::generate() const {
    return generateUpdate();
}

std::vector<std::string> UpdateSequenceGenerator::getPaths() const {
    return this->_paths;
}

UpdateSequenceGenerator::UpdateSequenceGenerator(std::set<StringData> fields,
                                                 std::size_t depth,
                                                 std::size_t length)
    : _fields(fields),
      _depth(depth),
      _length(length),
      _random(std::unique_ptr<SecureRandom>(SecureRandom::create())->nextInt64()) {
    auto path = "";
    _generatePaths(fields, depth, length, path);
    // Creates the same shuffle each time, but we don't care. We want to mess up the DFS ordering.
    std::random_shuffle(this->_paths.begin(), this->_paths.end(), this->_random);
}

}  // namespace mongo
