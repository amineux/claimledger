# GnuCOBOL ledger. `make` is a no-op-with-message if cobc is missing.
COBC ?= cobc
SRC  := src
COPY := copy
BIN  := bin

PROGRAMS := $(BIN)/post-citation $(BIN)/trial-balance $(BIN)/report

.PHONY: all clean check-cobc

all: check-cobc $(PROGRAMS)

check-cobc:
	@command -v $(COBC) >/dev/null 2>&1 || { \
	  echo "cobc not found. Install GnuCOBOL, or use ../scripts/verify_ledger.py"; \
	  echo "Sources remain the source of truth for the posting rules."; \
	  exit 0; }

$(BIN):
	mkdir -p $(BIN)

$(BIN)/post-citation: $(SRC)/POST-CITATION.cbl $(COPY)/JOURNAL.cpy | $(BIN)
	@command -v $(COBC) >/dev/null 2>&1 && \
	  $(COBC) -free -I $(COPY) -x -o $@ $(SRC)/POST-CITATION.cbl || true

$(BIN)/trial-balance: $(SRC)/TRIAL-BALANCE.cbl $(COPY)/JOURNAL.cpy $(COPY)/ACCOUNT.cpy | $(BIN)
	@command -v $(COBC) >/dev/null 2>&1 && \
	  $(COBC) -free -I $(COPY) -x -o $@ $(SRC)/TRIAL-BALANCE.cbl || true

$(BIN)/report: $(SRC)/REPORT.cbl | $(BIN)
	@command -v $(COBC) >/dev/null 2>&1 && \
	  $(COBC) -free -I $(COPY) -x -o $@ $(SRC)/REPORT.cbl || true

clean:
	rm -rf $(BIN)
