/* Stub at-spi2 accessibility bridge for wasm: no AT-SPI/D-Bus in the sandbox. GTK calls
 * atk_bridge_adaptor_init() at startup; these no-ops let GTK link + run without accessibility. */
#ifndef WASM_ATK_BRIDGE_H
#define WASM_ATK_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
int atk_bridge_adaptor_init(int *argc, char ***argv);
void atk_bridge_adaptor_cleanup(void);
#ifdef __cplusplus
}
#endif
#endif
