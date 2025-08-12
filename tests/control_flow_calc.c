// these should all print 0,1,2,3,4,...
{
  // test that block statements all get executed
  5 + 7  - 12;
  2 * 4 + 3 - 10;


  // nop should be legal but not do anything
  ; ; ;
}


// test that free statements all get executed
1 + 5 - 4;


12 + 17 - 26;




// nop should be legal but not do anything
; ; ; ;






loop {
   4;
   5;
   break;
   12000;
}


if(72) { 6; }
if(10 - 10) { 10023020; }
if(0) { 343284378; } else { 7; }
if(72) { 8; } else { 8349788; }


loop {
   9;
   10;
//     if(!100) { break; }
   11;
   break;
}
12;


if(!100) { 12532233; }
if(!100) { 25323423; } else { 13; }


loop {
   14;
   15;
   if(!100) { break; }
   { 16; 17; 18; 19; 1 + 19; }
   break;
}
21 + 343 / 2 - 343 / 2;