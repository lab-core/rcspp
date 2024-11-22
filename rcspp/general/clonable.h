#pragma once

#include <memory>


template<class Derived, class Base, class ReturnType = Base>
class Clonable : public Base {
public:
  virtual std::unique_ptr<ReturnType> clone() const override {

    return std::make_unique<Derived>(static_cast<Derived const&>(*this));
  }
};