#pragma once
#include "label/label.h"


template<typename ResourceType>
struct Solution {

  Label<ResourceType>* label;
  double cost;

};