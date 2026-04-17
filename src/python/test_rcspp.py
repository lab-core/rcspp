#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa
from unicodedata import name

from import_rscpp_lib import import_rscpp_lib
import_rscpp_lib()

from vrp.instance_reader import InstanceReader

from vrp.vrp import VRP
from vrp.dual_box_vrp import DualBoxVRP
from solution_formatter import format_solution
from utils import add_gaussian_noise
import json

def vrp_dual_box_instance(instance, dual_box_centre, radius, penalty, save=False, dir=""):
    print("Construct VRP")
    vrp = DualBoxVRP(instance, dual_box_centre, radius, penalty)
    print("Construct VRP ...Done")

    solution = vrp.solve()

    formatted_solution = format_solution(
                solution,
                vrp._VRP__paths,
                vrp.depot_id_,
                instance_name=instance_name,
                author="arthus",
            )
    if save:
        with open(f"/home/jullarth/Documents/Solutions/{dir}/{instance.get_name()}.txt", "w") as f:
           f.write(formatted_solution)

    return vrp, solution

R101_dual_optimal_solution = {1: 0.032854030441683335, 2: 10.412550131590592, 3: 0.7300667720610932, 4: 14.93845246985353, 5: 11.437119666629261, 6: 22.360679774997898, 7: 16.695383097886896, 8: 22.822810509050328, 9: 17.668217157041685, 10: 18.42402975609845, 11: 36.854255688550765, 12: 14.759628027757834, 13: 0.37881359084654775, 14: 17.983758891860255, 15: 28.33767935383692, 16: 16.55431346480279, 17: 15.41027097521166, 18: 5.643166555396945, 19: 25.828899597677037, 20: 25.969797041245926, 21: 13.918484301686494, 22: 26.311995581413527, 23: 10.702830330430956, 24: 17.196491498017238, 25: 18.541019662496836, 26: 14.770461680797919, 27: 0.18486144410054806, 28: 3.746267486103811, 29: 23.53200955940409, 30: 16.202016784325966, 31: 15.381470608902525, 32: 18.800532573023066, 33: 13.50941312748579, 34: 18.470334794173027, 35: 27.94808016418129, 36: 19.579608472353527, 37: 9.525649582087727, 38: 41.02198256170436, 39: 11.537598988230528, 40: 13.416407864998735, 41: 33.27580151909285, 42: 4.871391429110901, 43: 23.860493055038646, 44: 17.621984088602964, 45: 20.967540949200522, 46: 18.245965591226792, 47: 30.60241476378781, 48: 6.255748192461198, 49: 28.98190531995428, 50: 12.734639813893274, 51: 15.204158587728227, 52: 13.286315049451723, 53: 8.94427190999916, 54: 27.916293298596813, 55: 16.157424986663827, 56: 10.786099538788932, 57: 14.370216114800058, 58: 0.8598568524964563, 59: 4.459029093372745, 60: 4.989665820307401, 61: 10.229758144466036, 62: 14.95450732707613, 63: 6.128894319353833, 64: 65.97920447007913, 65: 55.89015009928626, 66: 45.32844379991249, 67: 53.21968665469315, 68: 16.448590156747123, 69: 11.31253934284355, 70: 0.24796954769490398, 71: 4.399715793171126, 72: 5.0383737812415035, 73: 3.798492460081988, 74: 11.991154918837879, 75: 16.39797661816955, 76: 2.3062447516320645, 77: 2.827238493414974, 78: 22.8399825018167, 79: 25.690242527387355, 80: 0.31456756702186794, 81: 28.943919550858805, 82: 23.874299298029516, 83: 10.229758144466036, 84: 32.47421655086977, 85: 25.9808344591909, 86: 37.174126459563695, 87: 30.255152180604796, 88: 17.92515424571674, 89: 0.027628760817471232, 90: 7.0710678118654755, 91: 5.239433179324806, 92: 3.0877177700718903, 93: 3.8152348045290836, 94: 23.94766222502346, 95: 4.669700855561107, 96: 7.222742943681453, 97: 5.850656720874625, 98: 5.601208613436196, 99: 9.758227582958952, 100: 5.964699803301713}


if __name__ == "__main__":
    solutions = {}
    print("Read instance...")
    instance_name = "R101"
    instance_path = "../../instances/" + instance_name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()
    dual_optimal_solution = instance_reader.read_dual_optimal(instance_name, "/home/jullarth/Documents/Solutions/Multiple_instances/")

    radius = 1
    penalties = [100]

    for penalty in penalties:
        verif_vrp , _ = vrp_dual_box_instance(instance, dual_optimal_solution, radius, 0.0)
        equalities = []

        for i in range(5):
            dual_box_centre = add_gaussian_noise(dual_optimal_solution, 0.0, 0.5)
            vrp, solution = vrp_dual_box_instance(instance, dual_box_centre, radius, penalty)

            solutions[instance_name+f"_{i}"] = {"penalty": penalty, 
                            "n_iter": vrp.get_n_iterations(), 
                            "total_time": vrp.get_total_problem_time(),
                            "lp_cost": vrp.get_lp_cost(),
                            "solution_cost": solution.cost,
                            "time_ratio": vrp.get_total_subproblem_time()/vrp.get_total_problem_time(),
                            "nb_added_columns": len(vrp._VRP__paths)
                            }
        
            penalty_lp_cost = vrp.get_lp_cost()
            equalities.append(penalty_lp_cost == verif_vrp.get_lp_cost())
    
    print(solutions)
    print(equalities)
    #with open(f"/home/jullarth/Documents/Solutions/Smoothing/summary.json", "a") as f:
    #    json.dump(solutions, f, indent=4)