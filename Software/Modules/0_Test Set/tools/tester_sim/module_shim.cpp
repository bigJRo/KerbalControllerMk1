// Linked into every simulated module: C entry points the simulation core finds
// with dlsym. The module sketch's setup()/loop() are renamed at compile time.
void module_setup();
void module_loop();
extern "C" void sim_module_setup() { module_setup(); }
extern "C" void sim_module_loop() { module_loop(); }
