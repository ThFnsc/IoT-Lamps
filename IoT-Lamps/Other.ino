int translate(int inPin) {
  switch (inPin) {
    case 0:
      return 16;
    case 1:
      return 5;
    case 2:
      return 4;
    case 3:
      return 0;
    case 4:
      return 2;
    case 5:
      return 14;
    case 6:
      return 12;
    case 7:
      return 13;
    case 8:
      return 15;
  }
}

String boolToOnOff(bool value) {
  return value ? "ON" : "OFF";
}

