# CMakeList.txt : projet CMake pour rcspp, incluez la source et définissez
# la logique spécifique au projet ici.
#
cmake_minimum_required (VERSION 3.26)

# Activez Rechargement à chaud pour les compilateurs MSVC si cela est pris en charge.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("rcspp")

include_directories ("/")
include_directories ("/resource/")

# Ajoutez une source à l'exécutable de ce projet.
# add_executable (rcspp "rcspp.cpp" "rcspp.h" "graph/graph.h" "graph/arc.h" "graph/node.h" "resource/resource.h" "algorithm/algorithm.h" "label/label.h" "resource/expansion/expansion_function.h" "resource/cost/cost_function.h" "resource/resource_function/feasibility/feasibility_function.h" "resource/resource.cpp" "resource/resource_composition.h" "resource/resource_composition.cpp" "resource/concrete/real_resource.h" "resource/concrete/real_resource.cpp"   "resource/resource_function/cost/trivial_cost_function.h" "resource/resource_function/cost/trivial_cost_function.cpp" "resource/concrete/cost/real_cost_function.h" "resource/concrete/cost/real_cost_function.cpp"  "resource/concrete/feasibility/min_max_feasibility_function.h" "resource/concrete/feasibility/min_max_feasibility_function.cpp" "resource/concrete/feasibility/real_feasibility_function.h" "resource/concrete/expansion/real_expansion_function.h" "resource/concrete/expansion/real_expansion_function.cpp" "resource/concrete/feasibility/real_feasibility_function.cpp" "resource/resource_function/feasibility/trivial_feasibility_function.h" "resource/resource_function/feasibility/trivial_feasibility_function.cpp" "resource/resource_function/expansion/trivial_expansion_function.h" "resource/resource_function/expansion/trivial_expansion_function.cpp" "resource/concrete/expansion/real_addition_expansion_function.h" "resource/concrete/expansion/real_addition_expansion_function.cpp" "resource/resource_function/factory/resource_function_factory.h")
add_executable (rcspp 
	"rcspp.cpp" 
	 
	 
	"resource/concrete/real_resource.cpp"
	"resource/concrete/resource_function/cost/real_value_cost_function.cpp"
	"resource/concrete/resource_function/expansion/real_addition_expansion_function.cpp"
	
	"resource/concrete/resource_function/feasibility/min_max_feasibility_function.cpp"
	
	
	
	 
	"general/clonable.h" 
	 
	   "resource/resource_function/dominance/dominance_function.h"  "resource/concrete/resource_function/dominance/real_value_dominance_function.h" "resource/concrete/resource_function/dominance/real_value_dominance_function.cpp" "resource/resource_function/dominance/trivial_dominance_function.h"  "resource/composition/cost/composition_cost_function.h"  "resource/composition/dominance/composition_dominance_function.h"  "resource/composition/expansion/composition_expansion_function.h"  "resource/composition/feasibility/composition_feasibility_function.h"     "solver/rcspp_solver.h"  "label/label_pool.h" "label/label_factory.h"  "resource/resource_factory.h"   "algorithm/solution.h" "algorithm/dominance_algorithm.h"    "resource/concrete/real_resource_factory.h" "resource/concrete/real_resource_factory.cpp"  "resource/composition/resource_composition_factory.h"    )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET rcspp PROPERTY CXX_STANDARD 23)

  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
