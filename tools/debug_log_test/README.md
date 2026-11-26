Computer tests for the debug log (no remote needed).

    g++ -std=c++17 -fsanitize=address,undefined -I../../remote test.cpp -o /tmp/dlt && /tmp/dlt
    g++ -std=c++17 -fsanitize=address,undefined -Istubs -I../../remote sim.cpp -o /tmp/dls && /tmp/dls
    node zip.test.cjs
