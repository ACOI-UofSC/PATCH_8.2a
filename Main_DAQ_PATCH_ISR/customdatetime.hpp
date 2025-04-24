class CustomDateTime {
public:
  int year, month, day, hour, minute, second;

  // Constructor to initialize date and time
  CustomDateTime(int y, int m, int d, int h, int min, int sec) {
    year = y;
    month = m;
    day = d;
    hour = h;
    minute = min;
    second = sec;
  }

  CustomDateTime() {
    
  }

  // Manually increase the seconds, and auto-handle date/time increment
  void incrementSeconds(int secsToAdd) {
    second += secsToAdd;

    // Handle seconds overflow
    while (second >= 60) {
      second -= 60;
      incrementMinute();
    }
  }

  // Increment minutes and auto-handle hour/day/month/year increment
  void incrementMinute() {
    minute++;

    // Handle minute overflow
    if (minute >= 60) {
      minute = 0;
      incrementHour();
    }
  }

  // Increment hours and auto-handle day/month/year increment
  void incrementHour() {
    hour++;

    // Handle hour overflow
    if (hour >= 24) {
      hour = 0;
      incrementDay();
    }
  }

  // Increment day and auto-handle month/year increment
  void incrementDay() {
    day++;

    // Handle days in each month
    if (day > daysInMonth(month, year)) {
      day = 1;
      incrementMonth();
    }
  }

  // Increment month and auto-handle year increment
  void incrementMonth() {
    month++;

    // Handle month overflow
    if (month > 12) {
      month = 1;
      incrementYear();
    }
  }

  // Increment year
  void incrementYear() {
    year++;
  }

  // Function to get the number of days in a month
  int daysInMonth(int month, int year) {
    // Handle months with fixed number of days
    if (month == 4 || month == 6 || month == 9 || month == 11) {
      return 30;
    }
    // Handle February (leap year)
    if (month == 2) {
      if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 29;  // Leap year
      } else {
        return 28;  // Non-leap year
      }
    }
    // Handle months with 31 days
    return 31;
  }

  CustomDateTime now() {
    return CustomDateTime(year, month, day, hour, minute, second);
  }  
};