/*
MIT License

Copyright (c) 2020 Christian Greyeyes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "md_parseman/MDParseMan.h"
#if defined(__clang__) && defined(__INTELLISENSE__)
#include <ciso646>
#endif
#include <cassert>

namespace {
	struct ResTuple {
		std::list<MDPMNode>::iterator value;
		const std::list<MDPMNode>::iterator* const toParent;
		bool ba;
		std::optional<bool> bb;
	};

	ResTuple lookAhead(const std::list<MDPMNode>::iterator& curr, const std::vector<std::list<MDPMNode>::iterator>& parents, bool retracting) noexcept;

	template<typename C=decltype(MDPMNode::children)>
	typename C::iterator blockSiblings(MDPMNode& n) noexcept;

	template<typename C=decltype(MDPMNode::children)>
	typename C::iterator blockSiblingEnd(MDPMNode& n) noexcept;
}

MDPMNode::public_iterator::self_type& MDPMNode::public_iterator::operator++() noexcept {
	pointer_type ptr = std::holds_alternative<pointer_type>(_valPtr) ? std::get<0>(_valPtr) : &*std::get<1>(_valPtr);
		auto [next, toAddToContainer, toPopContainer, toRetract] = lookAhead(std::get<1>(_valPtr), _parents, _retracting);

		if (not *toRetract) {
			if (toAddToContainer) {
				assert(not toPopContainer);
				_parents.emplace_back(*toAddToContainer);
			}
			_valPtr = next;
		}

		else if (toPopContainer and not _parents.empty()) {
			_valPtr = _parents.back();
			_parents.pop_back();
		}
		else if (*toRetract and not toPopContainer and not _parents.empty()) {
			// neither traverse leaf-ward nor root-ward; point to same node once more
		}
		else {
			_valPtr = ptr->parent;
		}
		_retracting = *toRetract;

#if 0
	if ( !_retracting and !ptr->children.empty()) {
		if (ptr->flavor != MDPMNode::type_e::Document) {
			_parents.emplace_back( std::move(std::get<1>(_valPtr)) );
			_valPtr = _parents.back()->children.begin();
		}
		else {
			//_root = ptr;
			_valPtr = ptr->children.begin();
		}
	}
	else if (!_retracting and ptr->children.empty() and not ptr->isPrintableBlock()) {
		_retracting = true;
	}
	else if (auto itPtr = std::get_if<storage_it_t>(&_valPtr); itPtr and (*itPtr != ptr->parent->children.end() and (++const_storage_it_t{*itPtr}) != ptr->parent->children.end())) {
		_retracting = false;
		++std::get<1>(_valPtr);
	}
	else if (!_parents.empty()) {
		_retracting = true;
		_valPtr = std::move(_parents.back());
		_parents.pop_back();
	}
	else if (ptr->parent) {
		_retracting = true;
		_valPtr = ptr->parent;
	}
#endif
	return *this;
}

MDPMNode::public_iterator::self_type MDPMNode::public_iterator::operator++(int) noexcept {
	self_type itCopy = *this;
	++(*this);
	return itCopy;
}

const MDPMNode::public_iterator::pointer_type MDPMNode::public_iterator::peekNext() const noexcept {
	auto [possibleVal, _1, _2, retracting] = lookAhead(std::get<1>(_valPtr), _parents, _retracting);
	if (*retracting) {
		return _parents.empty() ? std::get<1>(_valPtr)->parent : &*_parents.back();
	}
	return &*possibleVal;
}

bool MDPMNode::public_iterator::isSameBlockTypeIfNextSibling() const noexcept {
	return std::holds_alternative<storage_it_t>(_valPtr) and (
		++const_storage_it_t{ std::get<storage_it_t>(_valPtr) } != std::get<storage_it_t>(_valPtr)->parent->children.cend() and
		std::get<storage_it_t>(_valPtr)->flavor == (++const_storage_it_t{ std::get<storage_it_t>(_valPtr) })->flavor
		);
}
MDPMNode::public_iterator::BlockType MDPMNode::public_iterator::nextBlockIsParagraph() const noexcept {
	const auto& nextEl = *this->peekNext();
	return nextEl.isLeaf() ? BlockType{true,  nextEl.flavor } : BlockType{false, nextEl.flavor};
}

MDPMNode::public_iterator md_parseman::Parser::begin() noexcept {
	return { document_->parent->children.begin() };
}

MDPMNode::public_iterator md_parseman::Parser::end() noexcept {
	return {document_->parent};
}

namespace {
	ResTuple lookAhead(const std::list<MDPMNode>::iterator& curr, const std::vector<std::list<MDPMNode>::iterator>& parents, const bool retracting) noexcept {
		if (not retracting and !curr->children.empty()) {
#if 0
			if (curr->flavor != MDPMNode::type_e::Document) {
				parents.emplace_back(std::get<1>(_valPtr));
				_valPtr = parents.back()->children.begin();
			}
			else {
				//_root = ptr;
				_valPtr = curr->children.begin();
			}
#endif
			return { curr->children.begin(), &curr, false, retracting };
		}
		else if (not retracting and curr->children.empty() and not curr->isPrintableBlock()) {
			// empty container; push it into parents (top of parents_ stack usually never holds current object)
			return { curr, nullptr, false, true };
		}
		else if (curr != blockSiblingEnd(*curr) and (++std::list<MDPMNode>::const_iterator{curr}) != blockSiblingEnd(*curr)) {
			return { ++std::list<MDPMNode>::iterator{curr}, nullptr, false, false };
		}
		else if (!parents.empty()) {
			return { parents.back(), nullptr, true, true };
		}
		else if (curr->parent) {
			return { std::list<MDPMNode>::iterator{}, nullptr, false, true };//true };
		}
		else {
			return { std::list<MDPMNode>::iterator{}, nullptr, true, std::optional<bool>{} };
		}
	}
}

namespace {
	template<typename C>
	typename C::iterator blockSiblings(MDPMNode& n) noexcept {
		return n.parent->children.begin();
	}

	template<typename C>
	typename C::iterator blockSiblingEnd(MDPMNode& n) noexcept {
		return n.parent->children.end();
	}

}
