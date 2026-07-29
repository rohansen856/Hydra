int main() {
  while (true) {
    volatile double x = 0;
    for (int i = 0; i < 1000000; ++i) {
      x += i * 0.001;
    }
  }
  return 0;
}
