add(a, b) {
   if 0 == a { return b; }
   let temp;
   temp = add(0, a);
   let temp2;
   temp2 = add(0, b);
   return temp + (7 + temp2) - 7;
}
fib(a, b[1]) {
   let res;


   res = b[0];


   b[0] =  add(a, b[0]);


   return res;
}
main() {
   let first;
   let second[1];


   first = 1;
   second[0] = 1;


   loop {
       if second[0] > 10000 { return (second[0] * first) % 256; }


       first = fib(first, second);
   }


   return 1;
}


// returned value should be:   154
