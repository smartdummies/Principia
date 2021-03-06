(* ::Package:: *)

SetDirectory[NotebookDirectory[]];
<<"integration.wl";
Run[FileNameJoin[{"..", "Release", "x64", "mathematica.exe"}]];
<<"simple_harmonic_motion_graphs.generated.wl";
plot=IntegrationErrorPlot[eErrorData, names, "maximal energy error"];
Export["shm_energy_error.cdf", plot];
Export["shm_energy_error.png", plot];
plot=IntegrationErrorPlot[qErrorData, names, "maximal position error"];
Export["shm_position_error.cdf", plot];
Export["shm_position_error.png", plot];
plot=IntegrationErrorPlot[vErrorData, names, "maximal velocity error"];
Export["shm_velocity_error.cdf", plot];
Export["shm_velocity_error.png", plot];
<<"kepler_problem_graphs_0.000000.generated.wl";
plot=IntegrationErrorPlot[eErrorData, names, "maximal energy error"];
Export["kepler_energy_error_circular.cdf", plot];
Export["kepler_energy_error_circular.png", plot];
plot=IntegrationErrorPlot[qErrorData, names, "maximal position error"];
Export["kepler_position_error_circular.cdf", plot];
Export["kepler_position_error_circular.png", plot];
plot=IntegrationErrorPlot[vErrorData, names, "maximal velocity error"];
Export["kepler_velocity_error_circular.cdf", plot];
Export["kepler_velocity_error_circular.png", plot];
<<"kepler_problem_graphs_0.250000.generated.wl";
plot=IntegrationErrorPlot[eErrorData, names, "maximal energy error"];
Export["kepler_energy_error_pluto.cdf", plot];
Export["kepler_energy_error_pluto.png", plot];
plot=IntegrationErrorPlot[qErrorData, names, "maximal position error"];
Export["kepler_position_error_pluto.cdf", plot];
Export["kepler_position_error_pluto.png", plot];
plot=IntegrationErrorPlot[vErrorData, names, "maximal velocity error"];
Export["kepler_velocity_error_pluto.cdf", plot];
Export["kepler_velocity_error_pluto.png", plot];
<<"kepler_problem_graphs_0.640000.generated.wl";
plot=IntegrationErrorPlot[eErrorData, names, "maximal energy error"];
Export["kepler_energy_error_67p.cdf", plot];
Export["kepler_energy_error_67p.png", plot];
plot=IntegrationErrorPlot[qErrorData, names, "maximal position error"];
Export["kepler_position_error_67p.cdf", plot];
Export["kepler_position_error_67p.png", plot];
plot=IntegrationErrorPlot[vErrorData, names, "maximal velocity error"];
Export["kepler_velocity_error_67p.cdf", plot];
Export["kepler_velocity_error_67p.png", plot];
<<"kepler_problem_graphs_0.967000.generated.wl";
plot=IntegrationErrorPlot[eErrorData, names, "maximal energy error"];
Export["kepler_energy_error_1p.cdf", plot];
Export["kepler_energy_error_1p.png", plot];
plot=IntegrationErrorPlot[qErrorData, names, "maximal position error"];
Export["kepler_position_error_1p.cdf", plot];
Export["kepler_position_error_1p.png", plot];
plot=IntegrationErrorPlot[vErrorData, names, "maximal velocity error"];
Export["kepler_velocity_error_1p.cdf", plot];
Export["kepler_velocity_error_1p.png", plot];
