foo (x, y[4]) {
   putch(x);
   write(y[1]);
   write(y[1]);


   return 1337;
}


disp(a) {
   putch(a);
   putch(65);
   putch(a);
   putch(10); // AAA\n


   let x;
   let z[4];
   x = a;
   z[0] = 456;
   write(z[0]);
   putch(10); //456\n
   z[1] = 678;
   z[2] = foo(x, z);
   putch(10); // A678678\n


   write(z[2]);
   putch(10);  // 1337\n


   putch(10);


   return 0;
}


main() {
   let count;
   count = 3;


   let edge_case;
   edge_case = 6 || 4 && 2; // An easy case to mis generate


   loop {
       if !count { break; }
       count = count - 1;


       disp(65);
   }


   count = read();
   putch(count);   // echos as char
   putch(10);
   write(count); // echos a number, e.g. 65 -> A\n65\n
   putch(10);


   return 0;
}
