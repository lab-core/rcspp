// rcspp.cpp : définit le point d'entrée de l'application.
//

#include "rcspp.h"
#include "graph/graph.h"
#include "resource/concrete/real_resource_factory.h"
#include "algorithm/dominance_algorithm.h"
#include "solver/rcspp_solver.h"


std::unique_ptr<Graph> generate_graph(RealResourceFactory& resource_factory) {

	std::cout << "generate_graph" << std::endl;

	auto graph = std::make_unique<Graph>();

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

	std::cout << "graph: real_resource1=" << real_resource1->get_cost() << std::endl;

	graph->add_arc(0, 1, std::move(real_resource1));
	graph->add_arc(1, 2, std::move(real_resource2));
	graph->add_arc(1, 3, std::move(real_resource3));
	graph->add_arc(1, 10, std::move(real_resource4));
	graph->add_arc(2, 10, std::move(real_resource5));
	graph->add_arc(3, 10, std::move(real_resource6));	

	return graph;
}

void test_resource() {

	std::cout << "test_resource()\n";

	RealResource real_resource1(10);

	RealResource real_resource2(20);

	RealResource expanded_resource(100);

	std::cout << "real_resource1 <= real_resource2: " << (real_resource1 <= real_resource2) << "\n";

	std::cout << "real_resource2 <= real_resource1: " << (real_resource2 <= real_resource1) << "\n";

	real_resource1.expand(real_resource2, expanded_resource);


	std::cout << "real_resource1.get_value(): " << real_resource1.get_value() << "\n";

	std::cout << "real_resource2.get_value(): " << real_resource2.get_value() << "\n";

	std::cout << "real_resource1.is_feasible(): " << real_resource1.is_feasible() << "\n";

	std::cout << "real_resource2.is_feasible(): " << real_resource2.is_feasible() << "\n";

	std::cout << "real_resource1.get_cost(): " << real_resource1.get_cost() << "\n";

	std::cout << "real_resource2.get_cost(): " << real_resource2.get_cost() << "\n";

	std::cout << "expanded_resource.is_feasible(): " << expanded_resource.is_feasible() << "\n";

	std::cout << "expanded_resource.get_cost(): " << expanded_resource.get_cost() << "\n";

	std::vector<std::unique_ptr<Resource>> resource_components;
	resource_components.push_back(std::make_unique<RealResource>(3));
	resource_components.push_back(std::make_unique<RealResource>(8));

	auto res_composition = ResourceComposition(resource_components);


	std::vector<std::unique_ptr<Resource>> resource_components2;
	resource_components2.push_back(std::make_unique<RealResource>(13));
	resource_components2.push_back(std::make_unique<RealResource>(18));

	auto res_composition2 = ResourceComposition(resource_components2);

	std::vector<std::unique_ptr<Resource>> expanded_resource_components;
	expanded_resource_components.push_back(std::make_unique<RealResource>(100));
	expanded_resource_components.push_back(std::make_unique<RealResource>(100));

	auto expanded_res_composition = ResourceComposition(expanded_resource_components);

	std::cout << "res_composition.get_value(): " << res_composition.get_cost() << "\n";
	std::cout << "res_composition2.get_value(): " << res_composition2.get_cost() << "\n";

	std::cout << "res_composition.is_feasible(): " << res_composition.is_feasible() << "\n";

	std::cout << "res_composition2.is_feasible(): " << res_composition2.is_feasible() << "\n";

	std::cout << "res_composition <= res_composition2: " << (res_composition <= res_composition2) << "\n";

	std::cout << "res_composition2 <= res_composition: " << (res_composition2 <= res_composition) << "\n";

	res_composition.expand(res_composition2, expanded_res_composition);

	std::cout << "expanded_res_composition.get_cost(): " << expanded_res_composition.get_cost() << "\n";

	std::cout << "expanded_res_composition.get_components()[0]: " << expanded_res_composition.get_components()[0] << "\n";

	std::cout << "expanded_res_composition.get_components()[1]: " << expanded_res_composition.get_components()[1] << "\n";


	std::cout << "END: test_resource()\n";
}

void test_graph(Graph& graph) {

	auto& node1 = graph.get_node(0);
	auto& node2 = graph.get_node(1);
	auto& node3 = graph.get_node(10);

	std::cout << node1.id << ": in_arcs.size()=" << node1.in_arcs.size() << " | out_arcs.size()=" << node1.out_arcs.size() << std::endl;
	std::cout << node2.id << ": in_arcs.size()=" << node2.in_arcs.size() << " | out_arcs.size()=" << node2.out_arcs.size() << std::endl;
	std::cout << node3.id << ": in_arcs.size()=" << node3.in_arcs.size() << " | out_arcs.size()=" << node3.out_arcs.size() << std::endl;

	std::cout << "get_source_node_ids(): ";
	for (auto id : graph.get_source_node_ids()) {
		std::cout << id << " ";
	}
	std::cout << std::endl;

	std::cout << "get_sink_node_ids(): ";
	for (auto id : graph.get_sink_node_ids()) {
		std::cout << id << " ";
	}
	std::cout << std::endl;

	std::cout << "get_node_ids(): ";
	for (auto id : graph.get_node_ids()) {
		std::cout << id << " ";
	}
	std::cout << std::endl;

	std::cout << "get_arc_ids(): ";
	for (auto id : graph.get_arc_ids()) {
		std::cout << id << ": ";
		auto& arc = graph.get_arc(id);
		std::cout << "arc.id=" << arc.id << std::endl;
		std::cout << "arc.origin.id=" << arc.origin.id << std::endl;
		std::cout << "arc.destination.id=" << arc.destination.id << std::endl;
		std::cout << "arc.resource.get_cost()=" << arc.resource->get_cost() << std::endl;
	}
	std::cout << std::endl;
}

void test_label(RealResourceFactory& resource_factory, const Graph& graph) {
	std::cout << "test_label\n";

	LabelFactory label_factory(resource_factory);

	auto new_label1 = label_factory.make_label(0);
	auto new_label2 = label_factory.make_label(1);

	auto new_label_expanded = label_factory.make_label(2);

	std::cout << new_label1->id << ": new_label1->get_cost()=" << new_label1->get_cost() << std::endl;
	std::cout << new_label2->id << ": new_label2->get_cost()=" << new_label2->get_cost() << std::endl;
	std::cout << new_label_expanded->id << ": new_label_expanded->get_cost()=" << new_label_expanded->get_cost() << std::endl;

	std::cout << new_label1->id << "new_label1->is_feasible()=" << new_label1->is_feasible() << std::endl;
	std::cout << new_label2->id << "new_label2->is_feasible()=" << new_label2->is_feasible() << std::endl;
	std::cout << new_label_expanded->id << "new_label_expanded->is_feasible()=" << new_label_expanded->is_feasible() << std::endl;

	auto& arc0 = graph.get_arc(0);
	auto& arc1 = graph.get_arc(1);
	auto& arc2 = graph.get_arc(2);
	auto& arc3 = graph.get_arc(3);

	new_label1->expand(arc0, *new_label_expanded);
	std::cout << "arc0: new_label_expanded->get_cost()=" << new_label_expanded->get_cost() << std::endl;

	new_label1->expand(arc1, *new_label_expanded);
	std::cout << "arc1: new_label_expanded->get_cost()=" << new_label_expanded->get_cost() << std::endl;

	new_label1->expand(arc2, *new_label_expanded);
	std::cout << "arc2: new_label_expanded->get_cost()=" << new_label_expanded->get_cost() << std::endl;

	new_label1->expand(arc3, *new_label_expanded);
	std::cout << "arc3: new_label_expanded->get_cost()=" << new_label_expanded->get_cost() << std::endl;

	std::cout << "*new_label_expanded <= *new_label1: " << (*new_label_expanded <= *new_label1) << std::endl;

	std::cout << "*new_label1 <= *new_label2: " << (*new_label1 <= *new_label2) << std::endl;

}


int main()
{

	std::cout << "RCSPP\n";

	test_resource();

	RealResourceFactory resource_factory(std::make_unique<RealResource>(1));

	std::cout << "RealResourceFactory" << std::endl;

	auto graph = generate_graph(resource_factory);

	test_graph(*graph);

	test_label(resource_factory, *graph);
	
	auto algorithm = std::make_unique<DominanceAlgorithm>(resource_factory, *graph);

	RCSPPSolver solver(std::move(graph), std::move(algorithm));

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
	}

	return 0;
}
