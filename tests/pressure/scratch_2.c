char func(char x, char n ) {

  char returnSum; 
  char x0,x1,x2,x3;
  char y0,y1,y2,y3;
  char z0,z1,z2,z3;

  x1 = x2 = x3 = n; 
  y1 = y2 = y3 = x + 48;
  z1 = z2 = z3 = n + 31 ;

  x0 = x + 10 + n;
  y0 = 30 + (n<<2); 

  if (x0 > 50) {
    x1 += x0 - 3 + n; 
  }
  if (x1 > 20) {
    x1 += x0 - 3 + n; 
    x2 += x1 - 5 + n; 
  }
  if (x2 > 30) {
    x2 += x1 - 5 + n; 
    x3 += x2 - x + n; 
  }

  if (x3 > 7) {
    x3 += x0 + 2 + x;
    y0 += x3 + 4 + n + x0; 
  }

  if (y0 > 12) {
    y1 += y0 - 3 + n + x1;
  }
  
  if (y1 < -12) {
    y1 += y0 + x - 2 + x2; 
    y2 += y1 + n + 4 + x3;
  }

  if (y2 < 23) {
    y2 += y1 -17 +x; 
    y3 += y2 - n + (x-1); 
  }

  if (y3 > 16) {
    y3 += y0 - x + 18; 
    z0 += y3 - n + 12;
  }

  if (z0 < 11) {
    z1 += z0 - (n<<3) + 12; 
  }
  
  if (z1 > 3) {
    z1 += z0 - x + 2; 
    z2 += z1 - 4 + n; 
  }
  
  if (z2 > 19) {
    z2 += z1 - x + 35; 
    z3 += z2 + n - 15; 
  }

  return z3; 


}


