#include <stdio.h>

#include "disp-ho.h"
#include "str-util.h"

void print_n_and_k(const Dispersion& d) {
	complex n = d.n_value(190.0);
	printf("190nm: %g %g\n", n.real(), -n.imag());
	n = d.n_value(250.0);
	printf("250nm: %g %g\n", n.real(), -n.imag());
	n = d.n_value(633.0);
	printf("633nm: %g %g\n", n.real(), -n.imag());
}

int main() {
	HODispersion d{"HO test"};

	d.add_oscillator(HODispersion::Oscillator{150.0, 15.7, 0.0, 0.3333,  0.0});
	d.add_oscillator(HODispersion::Oscillator{10.0,  6.5,  0.3, 0.3333, -0.7});

	print_n_and_k(d);

	Writer w;
	d.write(w);
	w.save_tofile("ho-output.txt");

	str text;
	str_loadfile("ho-output.txt", &text);
	Lexer lexer(text.text());

	lexer.ident_to_store();
	lexer.string_to_store();
	str name = lexer.store;
	auto d_new = HODispersion::read(name.text(), lexer);
	print_n_and_k(*d_new);

	HODispersion d_ter(d);
	print_n_and_k(d_ter);

	return 0;
}
