#define SIGN (1 << 31)
#define P 17
#define Q 14
#define F (1 << Q) // 2^q

int int_to_fp(int n){
  /*
    we can convert an integer or real number into p.q format by multiplying with f.
  */
  return n * F;
}

int fp_to_int_round_zero(int fp){
  return fp / F;
}

int fp_to_int_round_near(int fp){
  /*
    To round to nearest, add f /2 to a positive number, or subtract it
    from a negative number, before dividing.
  */
  return fp >= 0 ? (fp + F / 2) / F : (fp - F / 2) / F;
}

int sum_int_fp(int n, int fp){
  return n * F + fp;
}

int diff_int_fp(int n, int fp){
  return n * F - fp;
}

int prod_int_fp(int n, int fp){
  return n * fp;
}

int div_int_fp(int n, int fp){
  return n / fp;
}

int sum_fp_fp(int x, int y){
  return x + y;
}

int diff_fp_fp(int x, int y){
  return x - y;
}

int prod_fp_fp(int x, int y){
  return ((int64_t)x) * y / F;
}

int div_fp_fp(int x, int y){
  return ((int64_t)x) * F / y;
}

