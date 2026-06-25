       IDENTIFICATION DIVISION.
       PROGRAM-ID. Age.
       AUTHOR. Fernando Brito.

       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  Age               PIC 99   VALUE ZEROS.
       01  Had_Birthday      PIC X    VALUE SPACES.
       01  Current_Year      PIC 9999 VALUE 2010.
       01  Result            PIC 9999 VALUE ZEROS.

       PROCEDURE DIVISION.
          DISPLAY "==> How old are you?".
          ACCEPT Age
          DISPLAY "==> Had you already had birthday this year (y or n)?".
          ACCEPT Had_Birthday

          SUBTRACT Current_Year FROM Age GIVING Result

          IF Had_Birthday = "n" THEN
            SUBTRACT 1 FROM Result GIVING Result
          END-IF

          DISPLAY "Let me guess... "" You were born in ", Result
          STOP RUN.
