
-- GQLITE-T-0525: appended to every functional test by `make test-functional`.
-- The stored graph the test leaves must be consistent (gql_check_storage()
-- gives []); -bail stops on the CHECK failure and prints the violations.
CREATE TEMP TABLE _gql_storage_ok(v TEXT CHECK (v = '[]'));
SELECT CASE WHEN gql_check_storage() <> '[]' THEN 'STORAGE INCONSISTENT: ' || gql_check_storage() END;
INSERT INTO _gql_storage_ok SELECT gql_check_storage();
