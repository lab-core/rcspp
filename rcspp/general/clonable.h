#pragma once

#include <memory>


template<class DerivedType, class BaseType, class ReturnType = BaseType>
class Clonable : public BaseType {
public:
  virtual std::unique_ptr<ReturnType> clone() const override {

    return std::make_unique<DerivedType>(static_cast<DerivedType const&>(*this));
  }
};