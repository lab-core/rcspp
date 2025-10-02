// rcspp.cpp : définit le point d'entrée de l'application.
//

#include "rcspp.h"
#include "algorithm/dominance_algorithm.h"
#include "graph/graph.h"
#include "resource/composition/resource_composition_factory.h"
#include "resource/concrete/real_resource_factory.h"
#include "resource/concrete/resource_function/cost/real_value_cost_function.h"
#include "resource/concrete/resource_function/dominance/real_value_dominance_function.h"
#include "solver/rcspp_solver.h"

std::unique_ptr<Graph<RealResource>>
generate_graph(RealResourceFactory &resource_factory) {

  std::cout << "generate_graph" << std::endl;

  auto graph = std::make_unique<Graph<RealResource>>();

  std::cout << "graph" << std::endl;

  auto real_resource1 = resource_factory.make_resource(1);

  std::cout << "real_resource1: " << real_resource1->is_feasible() << std::endl;

  auto real_resource2 = resource_factory.make_resource(2);

  std::cout << "real_resource2: " << real_resource2->is_feasible() << std::endl;

  auto real_resource3 = resource_factory.make_resource(3, 1, 5);

  std::cout << "real_resource3: " << real_resource3->is_feasible() << std::endl;

  auto real_resource4 = resource_factory.make_resource(40);

  std::cout << "real_resource4: " << real_resource4->is_feasible() << std::endl;

  auto real_resource5 = resource_factory.make_resource(5, 0, 4);

  std::cout << "real_resource5: " << real_resource5->is_feasible() << std::endl;

  auto real_resource6 = resource_factory.make_resource(6, 4, 6);

  std::cout << "real_resource6: " << real_resource6->is_feasible() << std::endl;

  graph->add_node(0, true, false);
  graph->add_node(10, false, true);
  graph->add_node(1);
  graph->add_node(2);
  graph->add_node(3);

  std::cout << "graph: real_resource1=" << real_resource1->get_cost()
            << std::endl;

  graph->add_arc(0, 1, std::move(real_resource1));
  graph->add_arc(1, 2, std::move(real_resource2));
  graph->add_arc(1, 3, std::move(real_resource3));
  graph->add_arc(1, 10, std::move(real_resource4));
  graph->add_arc(2, 10, std::move(real_resource5));
  graph->add_arc(3, 10, std::move(real_resource6));

  return graph;
}

// void test_resource() {
//
//	std::cout << "test_resource()\n";
//
//	RealResource real_resource1(10, 15, 50);
//
//	RealResource real_resource2(20, 15, 50);
//
//	RealResource expanded_resource(100);
//
//	std::cout << "real_resource1 <= real_resource2: " << (real_resource1 <=
//real_resource2) << "\n";
//
//	std::cout << "real_resource2 <= real_resource1: " << (real_resource2 <=
//real_resource1) << "\n";
//
//	real_resource1.expand(real_resource2, expanded_resource);
//
//
//	std::cout << "real_resource1.get_value(): " <<
//real_resource1.get_value() << "\n";
//
//	std::cout << "real_resource2.get_value(): " <<
//real_resource2.get_value() << "\n";
//
//	std::cout << "real_resource1.is_feasible(): " <<
//real_resource1.is_feasible() << "\n";
//
//	std::cout << "real_resource2.is_feasible(): " <<
//real_resource2.is_feasible() << "\n";
//
//	std::cout << "real_resource1.get_cost(): " << real_resource1.get_cost()
//<< "\n";
//
//	std::cout << "real_resource2.get_cost(): " << real_resource2.get_cost()
//<< "\n";
//
//	std::cout << "expanded_resource.is_feasible(): " <<
//expanded_resource.is_feasible() << "\n";
//
//	std::cout << "expanded_resource.get_cost(): " <<
//expanded_resource.get_cost() << "\n";
//
//	ResourceCompositionFactory<RealResource, RealResource>
//resource_composition_factory;
//
//	std::vector<std::unique_ptr<RealResource>> real_resources_vector;
//	real_resources_vector.emplace_back(std::make_unique<RealResource>(3, 10,
//50)); 	real_resources_vector.emplace_back(std::make_unique<RealResource>(8, 10,
//50));
//
//	std::vector<std::unique_ptr<RealResource>> second_real_resources_vector;
//	second_real_resources_vector.emplace_back(std::make_unique<RealResource>(23,
//10, 50));
//	second_real_resources_vector.emplace_back(std::make_unique<RealResource>(28,
//10, 50));
//	second_real_resources_vector.emplace_back(std::make_unique<RealResource>(29,
//10, 50));
//
//	std::tuple<std::vector<std::unique_ptr<RealResource>>,
//		std::vector<std::unique_ptr<RealResource>>> resource_components{
//		std::move(real_resources_vector),
//		std::move(second_real_resources_vector)
//	};
//
//	auto res_composition =
//resource_composition_factory.make_default_resource(resource_components);
//
//	const auto& res_comp_res_vec1 =
//std::get<0>(res_composition->get_components()); 	const auto& res_comp_res_vec2
//= std::get<1>(res_composition->get_components());
//
//	std::cout << "res_composition: \n";
//
//	for (const auto& res1 : res_comp_res_vec1) {
//		std::cout << "res1: " << res1->get_cost() << "\n";
//	}
//
//	for (const auto& res2 : res_comp_res_vec2) {
//		std::cout << "res2: " << res2->get_cost() << "\n";
//	}
//
//	std::cout << "res_composition->is_feasible(): " <<
//res_composition->is_feasible() << std::endl;
//
//
//	std::vector<std::unique_ptr<RealResource>> real_resources_vector2;
//	real_resources_vector2.emplace_back(std::make_unique<RealResource>(13,
//10, 50));
//	real_resources_vector2.emplace_back(std::make_unique<RealResource>(18,
//10, 50));
//
//	std::vector<std::unique_ptr<RealResource>>
//second_real_resources_vector2;
//	second_real_resources_vector2.emplace_back(std::make_unique<RealResource>(33,
//10, 50));
//	second_real_resources_vector2.emplace_back(std::make_unique<RealResource>(18,
//10, 50));
//	second_real_resources_vector2.emplace_back(std::make_unique<RealResource>(39,
//10, 50));
//
//	std::tuple<std::vector<std::unique_ptr<RealResource>>,
//		std::vector<std::unique_ptr<RealResource>>>
//resource_components2{ 		std::move(real_resources_vector2),
//		std::move(second_real_resources_vector2)
//	};
//
//	auto res_composition2 =
//resource_composition_factory.make_default_resource(resource_components2);
//
//	const auto& res_comp2_res_vec1 =
//std::get<0>(res_composition2->get_components()); 	const auto&
//res_comp2_res_vec2 = std::get<1>(res_composition2->get_components());
//
//	std::cout << "res_composition2: \n";
//
//	for (const auto& res1 : res_comp2_res_vec1) {
//		std::cout << "res1: " << res1->get_cost() << "\n";
//	}
//
//	for (const auto& res2 : res_comp2_res_vec2) {
//		std::cout << "res2: " << res2->get_cost() << "\n";
//	}
//
//	std::cout << "res_composition2->is_feasible(): " <<
//res_composition2->is_feasible() << std::endl;
//
//
//	std::vector<std::unique_ptr<RealResource>>
//expanded_real_resources_vector;
//	expanded_real_resources_vector.emplace_back(std::make_unique<RealResource>(5,
//10, 50));
//	expanded_real_resources_vector.emplace_back(std::make_unique<RealResource>(10,
//10, 50));
//
//	std::vector<std::unique_ptr<RealResource>>
//second_expanded_real_resources_vector;
//	second_expanded_real_resources_vector.emplace_back(std::make_unique<RealResource>(25,
//10, 50));
//	second_expanded_real_resources_vector.emplace_back(std::make_unique<RealResource>(30,
//10, 50));
//	second_expanded_real_resources_vector.emplace_back(std::make_unique<RealResource>(31,
//10, 50));
//
//	std::tuple<std::vector<std::unique_ptr<RealResource>>,
//		std::vector<std::unique_ptr<RealResource>>>
//expanded_resource_components{ 		std::move(expanded_real_resources_vector),
//		std::move(second_expanded_real_resources_vector)
//	};
//
//	auto expanded_res_composition =
//resource_composition_factory.make_default_resource(expanded_resource_components);
//
//
//
//	std::cout << "res_composition->get_value(): " <<
//res_composition->get_cost() << "\n"; 	std::cout <<
//"res_composition2->get_value(): " << res_composition2->get_cost() << "\n";
//
//	std::cout << "res_composition.is_feasible(): " <<
//res_composition->is_feasible() << "\n";
//
//	std::cout << "res_composition2.is_feasible(): " <<
//res_composition2->is_feasible() << "\n";
//
//	std::cout << "res_composition <= res_composition2: " <<
//(*res_composition <= *res_composition2) << "\n";
//
//	std::cout << "res_composition2 <= res_composition: " <<
//(*res_composition2 <= *res_composition) << "\n";
//
//	std::cout << "expanded_res_composition.get_cost(): " <<
//expanded_res_composition->get_cost() << "\n";
//
//	res_composition->expand(*res_composition2, *expanded_res_composition);
//
//	std::cout << "expanded_res_composition.get_cost(): " <<
//expanded_res_composition->get_cost() << "\n";
//
//	const auto& expanded_res_composition_components =
//expanded_res_composition->get_components();
//
//	std::cout << "res_composition <= expanded_res_composition: " <<
//(*res_composition <= *expanded_res_composition) << "\n";
//
//	std::cout << "res_composition2 <= expanded_res_composition: " <<
//(*res_composition2 <= *expanded_res_composition) << "\n";
//
//	const auto& expanded_res_vec1 =
//std::get<0>(expanded_res_composition_components); 	const auto&
//expanded_res_vec2 = std::get<1>(expanded_res_composition_components);
//
//	std::cout << "expanded_res_composition: \n";
//
//	for (const auto& res1 : expanded_res_vec1) {
//		std::cout << "res1: " << res1->get_cost() << "\n";
//	}
//
//	for (const auto& res2 : expanded_res_vec2) {
//		std::cout << "res2: " << res2->get_cost() << "\n";
//	}
//
//	std::cout << "expanded_res_composition->is_feasible(): " <<
//expanded_res_composition->is_feasible() << std::endl;
//
//	std::cout << "END: test_resource()\n";
// }

// void test_graph(Graph<RealResource>& graph) {
//
//	auto& node1 = graph.get_node(0);
//	auto& node2 = graph.get_node(1);
//	auto& node3 = graph.get_node(10);
//
//	std::cout << node1.id << ": in_arcs.size()=" << node1.in_arcs.size() <<
//" | out_arcs.size()=" << node1.out_arcs.size() << std::endl; 	std::cout <<
//node2.id << ": in_arcs.size()=" << node2.in_arcs.size() << " |
//out_arcs.size()=" << node2.out_arcs.size() << std::endl; 	std::cout << node3.id
//<< ": in_arcs.size()=" << node3.in_arcs.size() << " | out_arcs.size()=" <<
//node3.out_arcs.size() << std::endl;
//
//	std::cout << "get_source_node_ids(): ";
//	for (auto id : graph.get_source_node_ids()) {
//		std::cout << id << " ";
//	}
//	std::cout << std::endl;
//
//	std::cout << "get_sink_node_ids(): ";
//	for (auto id : graph.get_sink_node_ids()) {
//		std::cout << id << " ";
//	}
//	std::cout << std::endl;
//
//	std::cout << "get_node_ids(): ";
//	for (auto id : graph.get_node_ids()) {
//		std::cout << id << " ";
//	}
//	std::cout << std::endl;
//
//	std::cout << "get_arc_ids(): ";
//	for (auto id : graph.get_arc_ids()) {
//		std::cout << id << ": ";
//		auto& arc = graph.get_arc(id);
//		std::cout << "arc.id=" << arc.id << std::endl;
//		std::cout << "arc.origin.id=" << arc.origin.id << std::endl;
//		std::cout << "arc.destination.id=" << arc.destination.id <<
//std::endl; 		std::cout << "arc.resource.get_cost()=" << arc.resource->get_cost()
//<< std::endl;
//	}
//	std::cout << std::endl;
// }

// void test_label(RealResourceFactory& resource_factory, const
// Graph<RealResource>& graph) { 	std::cout << "test_label\n";
//
//	LabelFactory label_factory(resource_factory);
//
//	auto new_label1 = label_factory.make_label(0);
//	auto new_label2 = label_factory.make_label(1);
//
//	auto new_label_expanded = label_factory.make_label(2);
//
//	std::cout << new_label1->id << ": new_label1->get_cost()=" <<
//new_label1->get_cost() << std::endl; 	std::cout << new_label2->id << ":
//new_label2->get_cost()=" << new_label2->get_cost() << std::endl; 	std::cout <<
//new_label_expanded->id << ": new_label_expanded->get_cost()=" <<
//new_label_expanded->get_cost() << std::endl;
//
//	std::cout << new_label1->id << "new_label1->is_feasible()=" <<
//new_label1->is_feasible() << std::endl; 	std::cout << new_label2->id <<
//"new_label2->is_feasible()=" << new_label2->is_feasible() << std::endl;
//	std::cout << new_label_expanded->id <<
//"new_label_expanded->is_feasible()=" << new_label_expanded->is_feasible() <<
//std::endl;
//
//	auto& arc0 = graph.get_arc(0);
//	auto& arc1 = graph.get_arc(1);
//	auto& arc2 = graph.get_arc(2);
//	auto& arc3 = graph.get_arc(3);
//
//	new_label1->expand(arc0, *new_label_expanded);
//	std::cout << "arc0: new_label_expanded->get_cost()=" <<
//new_label_expanded->get_cost() << std::endl;
//
//	new_label1->expand(arc1, *new_label_expanded);
//	std::cout << "arc1: new_label_expanded->get_cost()=" <<
//new_label_expanded->get_cost() << std::endl;
//
//	new_label1->expand(arc2, *new_label_expanded);
//	std::cout << "arc2: new_label_expanded->get_cost()=" <<
//new_label_expanded->get_cost() << std::endl;
//
//	new_label1->expand(arc3, *new_label_expanded);
//	std::cout << "arc3: new_label_expanded->get_cost()=" <<
//new_label_expanded->get_cost() << std::endl;
//
//	std::cout << "*new_label_expanded <= *new_label1: " <<
//(*new_label_expanded <= *new_label1) << std::endl;
//
//	std::cout << "*new_label1 <= *new_label2: " << (*new_label1 <=
//*new_label2) << std::endl;
//
// }

// std::unique_ptr<ResourceComposition<RealResource, RealResource>>
// construct_resource_composition( 	std::vector<double> real_vector1,
//std::vector<double> real_vector2) {
//
//	RealResourceFactory
//real_resource_factory(std::make_unique<RealResource>(0));
//
//	std::vector<std::unique_ptr<RealResource>> real_resources_vector1;
//	for (auto real_number : real_vector1) {
//		real_resources_vector1.emplace_back(real_resource_factory.make_resource(real_number));
//	}
//
//	std::vector<std::unique_ptr<RealResource>> real_resources_vector2;
//	for (auto real_number : real_vector2) {
//		real_resources_vector2.emplace_back(real_resource_factory.make_resource(real_number));
//	}
//
//	std::tuple<std::vector<std::unique_ptr<RealResource>>,
//		std::vector<std::unique_ptr<RealResource>>> resource_components{
//		std::move(real_resources_vector1),
//		std::move(real_resources_vector2)
//	};
//
//	ResourceCompositionFactory<RealResource, RealResource> resource_factory;
//
//	auto new_resource =
//resource_factory.make_default_resource(resource_components);
//
//	return new_resource;
// }

std::unique_ptr<Graph<ResourceComposition<RealResource, RealResource>>>
generate_graph_composition(
    ResourceCompositionFactory<RealResource, RealResource> &resource_factory) {

  std::cout << "generate_graph_composition" << std::endl;

  auto graph = std::make_unique<
      Graph<ResourceComposition<RealResource, RealResource>>>();

  std::cout << "graph" << std::endl;

  auto resource1 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{1.1}, {2.3}},
           std::vector<std::tuple<double>>{{4.2}, {6.4}}});

  std::cout << "resource1: " << resource1->is_feasible() << std::endl;

  auto resource2 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{0.1}, {0.2}},
           std::vector<std::tuple<double>>{{7.4}, {3.5}}});

  std::cout << "_resource2: " << resource2->is_feasible() << std::endl;

  auto resource3 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{1.6}, {2.2}},
           std::vector<std::tuple<double>>{{2.0}, {6.5}}});

  std::cout << "resource3: " << resource3->is_feasible() << std::endl;

  auto resource4 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{4.1}, {5.2}},
           std::vector<std::tuple<double>>{{3.4}, {2.5}}});

  std::cout << "resource4: " << resource4->is_feasible() << std::endl;

  auto resource5 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{1.2}, {2.32}},
           std::vector<std::tuple<double>>{{1.4}, {7.5}}});

  std::cout << "resource5: " << resource5->is_feasible() << std::endl;

  auto resource6 =
      resource_factory.make_resource<std::tuple<double>, std::tuple<double>>(
          {std::vector<std::tuple<double>>{{9.3}, {2}},
           std::vector<std::tuple<double>>{{3.4}, {8.5}}});

  std::cout << "resource6: " << resource6->is_feasible() << std::endl;

  graph->add_node(0, true, false);
  graph->add_node(10, false, true);
  graph->add_node(1);
  graph->add_node(2);
  graph->add_node(3);

  std::cout << "graph: resource1=" << resource1->get_cost() << std::endl;

  graph->add_arc(0, 1, std::move(resource1));
  graph->add_arc(1, 2, std::move(resource2));
  graph->add_arc(1, 3, std::move(resource3));
  graph->add_arc(1, 10, std::move(resource4));
  graph->add_arc(2, 10, std::move(resource5));
  graph->add_arc(3, 10, std::move(resource6));

  return graph;
}

void test_graph_resource_composition() {

  std::cout << "test_graph_resource_composition\n";

  auto resource_composition =
      std::make_unique<ResourceComposition<RealResource, RealResource>>();
  auto &real_resource1_1 = resource_composition->add_component<0>(
      std::make_unique<RealResource>(0.2));
  auto &real_resource1_2 = resource_composition->add_component<0>(
      std::make_unique<RealResource>(0.5));

  auto &real_resource2_1 = resource_composition->add_component<1>(
      std::make_unique<RealResource>(1.3));
  auto &real_resource2_2 = resource_composition->add_component<1>(
      std::make_unique<RealResource>(1.6));
  auto &real_resource2_3 = resource_composition->add_component<1>(
      std::make_unique<RealResource>(1.4));

  ResourceCompositionFactory<RealResource, RealResource> resource_factory(
      std::move(resource_composition));

  auto graph = generate_graph_composition(resource_factory);

  auto algorithm = std::make_unique<
      DominanceAlgorithm<ResourceComposition<RealResource, RealResource>>>(
      resource_factory, *graph);

  RCSPPSolver<ResourceComposition<RealResource, RealResource>> solver(
      std::move(graph), std::move(algorithm));

  auto solutions = solver.solve();

  for (auto &solution : solutions) {
    std::cout << "solution.cost=" << solution.cost << std::endl;
    std::cout << "solution.label->id=" << solution.label->id << std::endl;

    std::cout << "Path nodes: ";
    for (auto node_id : solution.label->get_path_node_ids()) {
      std::cout << node_id << " -> ";
    }
    std::cout << std::endl;

    std::cout << "Path arcs: ";
    for (auto arc_id : solution.label->get_path_arc_ids()) {
      std::cout << arc_id << " -> ";
    }
    std::cout << std::endl;
  }
}

int main() {

  std::cout << "RCSPP\n";

  test_graph_resource_composition();

  auto resource_composition =
      std::make_unique<ResourceComposition<RealResource, RealResource>>();
  auto &real_resource1_1 = resource_composition->add_component<0, RealResource>(
      std::make_tuple<double>(10.2));
  auto &real_resource1_2 = resource_composition->add_component<0, RealResource>(
      std::make_tuple<double>(10.5));

  auto &real_resource2_1 = resource_composition->add_component<1, RealResource>(
      std::make_tuple<double>(21.3));
  auto &real_resource2_2 = resource_composition->add_component<1, RealResource>(
      std::make_tuple<double>(21.6));
  auto &real_resource2_3 = resource_composition->add_component<1, RealResource>(
      std::make_tuple<double>(21.4));

  std::cout << "real_resource1_1.get_cost()=" << real_resource1_1.get_cost()
            << std::endl;

  auto &resource_component_vector =
      resource_composition->get_components<1, RealResource>();

  std::cout << "resource_component_vector.size()="
            << resource_component_vector.size();

  std::cout
      << "real_resource1_1.get_cost()="
      << resource_composition->get_components<1, RealResource>(1).get_cost()
      << std::endl;

  ResourceCompositionFactory<RealResource, RealResource>
      resource_composition_factory(std::move(resource_composition));

  auto new_resource = resource_composition_factory.make_resource();

  std::cout << "new_resource->get_components<0, RealResource>(0)="
            << new_resource->get_components<0, RealResource>(0).get_cost()
            << std::endl;
  std::cout << "new_resource->get_components<0, RealResource>(0)="
            << new_resource->get_components<0, RealResource>(1).get_cost()
            << std::endl;
  std::cout << "new_resource->get_components<1, RealResource>(0)="
            << new_resource->get_components<1, RealResource>(0).get_cost()
            << std::endl;
  std::cout << "new_resource->get_components<1, RealResource>(1)="
            << new_resource->get_components<1, RealResource>(1).get_cost()
            << std::endl;
  std::cout << "new_resource->get_components<1, RealResource>(2)="
            << new_resource->get_components<1, RealResource>(2).get_cost()
            << std::endl;

  // test_resource();

  /*RealResourceFactory resource_factory(std::make_unique<RealResource>(1));

  std::cout << "RealResourceFactory" << std::endl;

  auto graph = generate_graph(resource_factory);

  test_graph(*graph);

  test_label(resource_factory, *graph);

  auto algorithm =
  std::make_unique<DominanceAlgorithm<RealResource>>(resource_factory, *graph);

  RCSPPSolver<RealResource> solver(std::move(graph), std::move(algorithm));

  auto solutions = solver.solve();

  for (auto& solution : solutions) {
          std::cout << "solution.cost=" << solution.cost << std::endl;
          std::cout << "solution.label->id=" << solution.label->id << std::endl;

          std::cout << "Path nodes: ";
          for (auto node_id : solution.label->get_path_node_ids()) {
                  std::cout << node_id << " -> ";
          }
          std::cout << std::endl;

          std::cout << "Path arcs: ";
          for (auto arc_id : solution.label->get_path_arc_ids()) {
                  std::cout << arc_id << " -> ";
          }
          std::cout << std::endl;
  }*/

  return 0;
}
