>>SOURCE FORMAT FREE
IDENTIFICATION DIVISION.
PROGRAM-ID. REPORT.
AUTHOR. CLAIMLEDGER.
REMARKS.
    Green-bar intellectual-debt report from trial_balance.csv.

ENVIRONMENT DIVISION.
INPUT-OUTPUT SECTION.
FILE-CONTROL.
    SELECT BALANCE-FILE ASSIGN TO "trial_balance.csv"
        ORGANIZATION IS LINE SEQUENTIAL
        FILE STATUS IS BAL-STATUS.
    SELECT REPORT-FILE ASSIGN TO "trial_balance.rpt"
        ORGANIZATION IS LINE SEQUENTIAL
        FILE STATUS IS RPT-STATUS.

DATA DIVISION.
FILE SECTION.
FD  BALANCE-FILE.
01  BAL-IN                    PIC X(128).

FD  REPORT-FILE.
01  RPT-LINE                  PIC X(80).

WORKING-STORAGE SECTION.
01  BAL-STATUS                PIC XX VALUE "00".
01  RPT-STATUS                PIC XX VALUE "00".
01  WS-EOF                    PIC X VALUE "N".
01  WS-SKIP                   PIC X VALUE "Y".
01  WS-ACCT                   PIC X(16).
01  WS-DEBIT-X                PIC X(16).
01  WS-CREDIT-X               PIC X(16).
01  WS-NET-X                  PIC X(16).
01  WS-PTR                    PIC 99.
01  WS-F                      PIC 99.
01  WS-FIELD                  PIC X(24).
01  WS-LINES                  PIC 9(4) VALUE 0.

PROCEDURE DIVISION.
MAIN.
    OPEN INPUT BALANCE-FILE
    IF BAL-STATUS NOT = "00"
        DISPLAY "REPORT: cannot open trial_balance.csv status=" BAL-STATUS
        STOP RUN
    END-IF
    OPEN OUTPUT REPORT-FILE
    MOVE "CLAIMLEDGER  INTELLECTUAL-DEBT TRIAL BALANCE" TO RPT-LINE
    WRITE RPT-LINE
    MOVE "ACCOUNT           DEBIT        CREDIT           NET" TO RPT-LINE
    WRITE RPT-LINE
    MOVE "---------------- ------------ ------------ ------------" TO RPT-LINE
    WRITE RPT-LINE
    PERFORM UNTIL WS-EOF = "Y"
        READ BALANCE-FILE
            AT END
                MOVE "Y" TO WS-EOF
            NOT AT END
                PERFORM EMIT
        END-READ
    END-PERFORM
    MOVE "---------------- ------------ ------------ ------------" TO RPT-LINE
    WRITE RPT-LINE
    MOVE "END OF REPORT — double-entry citation ledger" TO RPT-LINE
    WRITE RPT-LINE
    CLOSE BALANCE-FILE
    CLOSE REPORT-FILE
    DISPLAY "REPORT: wrote " WS-LINES " body lines to trial_balance.rpt"
    STOP RUN.

EMIT.
    IF WS-SKIP = "Y"
        MOVE "N" TO WS-SKIP
        EXIT PARAGRAPH
    END-IF
    IF BAL-IN = SPACES
        EXIT PARAGRAPH
    END-IF
    MOVE 1 TO WS-PTR
    MOVE 0 TO WS-F
    MOVE SPACES TO WS-ACCT WS-DEBIT-X WS-CREDIT-X WS-NET-X
    PERFORM SPLIT 4 TIMES
    MOVE SPACES TO RPT-LINE
    STRING
        WS-ACCT DELIMITED BY SIZE
        " " DELIMITED BY SIZE
        WS-DEBIT-X DELIMITED BY SIZE
        " " DELIMITED BY SIZE
        WS-CREDIT-X DELIMITED BY SIZE
        " " DELIMITED BY SIZE
        WS-NET-X DELIMITED BY SIZE
        INTO RPT-LINE
    END-STRING
    WRITE RPT-LINE
    ADD 1 TO WS-LINES.

SPLIT.
    MOVE SPACES TO WS-FIELD
    UNSTRING BAL-IN DELIMITED BY ","
        INTO WS-FIELD
        WITH POINTER WS-PTR
    END-UNSTRING
    ADD 1 TO WS-F
    IF WS-F = 1
        MOVE WS-FIELD TO WS-ACCT
    END-IF
    IF WS-F = 2
        MOVE WS-FIELD TO WS-DEBIT-X
    END-IF
    IF WS-F = 3
        MOVE WS-FIELD TO WS-CREDIT-X
    END-IF
    IF WS-F = 4
        MOVE WS-FIELD TO WS-NET-X
    END-IF.
