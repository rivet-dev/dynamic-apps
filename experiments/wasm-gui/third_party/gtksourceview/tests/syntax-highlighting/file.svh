class my_class extends some_class;

  // This is a comment.
  /* This is also a comment, but it containts keywords: bit string, etc */

  // Some types.
  string         my_string = "This is a string";
  bit [3:0]        my_bits = 4'b0z1x;
  integer       my_integer = 32'h0z2ab43x;
  real             my_real = 1.2124155e-123;
  shortreal   my_shortreal = -0.1111e1;
  int               my_int = 53152462;
 
  extern function bit
    my_function(
      int unsigned       something);

endclass : my_class

function bit
  my_class::my_function(
    int unsigned    something);

  /* Display a string.
   *
   *   This is a slightly awkward string as it has
   *   special characters and line continuations.
   */
  ("Display a string that continues over             multiple lines and contains special             characters: \n \t \" \'");

  // Use a system task.
  my_int = (my_bits);

  // ();     // Commenting a system task.
  // my_function();   // Commenting a function.

endfunction my_function

program test();

  my_class c;

  c = new();
  c.my_function(3);

endprogram : test
