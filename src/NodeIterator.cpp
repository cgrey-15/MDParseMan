#include "md_parseman/MDParseMan.h"

MDPMNode::public_iterator::self_type& MDPMNode::public_iterator::operator++() {
	pointer_type ptr = std::holds_alternative<pointer_type>(_valPtr) ? std::get<0>(_valPtr) : &*std::get<1>(_valPtr);
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
	else if (auto itPtr = std::get_if<std::list<MDPMNode>::iterator>(&_valPtr); itPtr and (*itPtr != ptr->parent->children.end() and (++std::list<MDPMNode>::const_iterator{*itPtr}) != ptr->parent->children.end())) {
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
	//++_itPos;
	return *this;//std::holds_alternative<pointer_type>(_valPtr) ? *std::get<0>(_valPtr) : *std::get<1>(_valPtr);
}

MDPMNode::public_iterator::self_type MDPMNode::public_iterator::operator++(int) {
	self_type itCopy = *this;
	++(*this);
	return itCopy;
}


MDPMNode::public_iterator md_parseman::Parser::begin() {
	//return MDPMNode::public_iterator{ std::variant<MDPMNode*, std::list<MDPMNode>::iterator>{root_}, nullptr, std::vector<std::list<MDPMNode>::iterator>{} };
	return { root_ };
	//return MDPMNode::public_iterator{};
	//return {};
}

MDPMNode::public_iterator md_parseman::Parser::end() {
	return {root_->parent};
}

/*
MDPMNode::public_iterator::self_type& MDPMNode::public_iterator::operator--() {

}
*/
