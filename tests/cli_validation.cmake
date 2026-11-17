# Check both exit status and diagnostic: a crash is not a successful rejection.
function(expect_invalid expected_message)
    execute_process(
        COMMAND "${DEMO}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT "${result}" STREQUAL "1" OR NOT error MATCHES "${expected_message}")
        message(FATAL_ERROR "Unexpected rejection for ${ARGN}: ${result}; ${output}; ${error}")
    endif()
endfunction()

expect_invalid("Invalid numeric value" --steps -1)
expect_invalid("Invalid numeric value" --steps 2x)
expect_invalid("Invalid numeric value" --steps 9999999999999999999999999)
expect_invalid("Time step must be finite" --dt nan)
expect_invalid("Time step must be finite" --dt inf)
expect_invalid("Time step must be finite" --dt 0)
expect_invalid("Time step must be finite" --dt -0.1)
expect_invalid("Thread count must be strictly positive" --solver openmp --threads 0)
expect_invalid("Thread count must be strictly positive" --solver openmp --threads -2)
expect_invalid("only valid with an OpenMP solver" --threads 2)
expect_invalid("Solver must be" --solver unknown)
expect_invalid("Case must be" --case unknown)
expect_invalid("Unknown option" --unknown value)
expect_invalid("Missing value" --dt)
