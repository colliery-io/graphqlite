---
title: Query Patterns in SQL
slug: 0.12.0/tutorials/sql-patterns
---

This tutorial covers intermediate and advanced Cypher patterns using GraphQLite from the SQLite CLI. The examples use a movie database: movies, actors, directors, and the relationships between them.

## Setup: Build the Movie Database

Save this block as `movie_setup.sql` and run it with `sqlite3 movies.db < movie_setup.sql`, or paste it into an interactive session after `.load build/graphqlite`.

```sql
.load build/graphqlite
.mode column
.headers on

-- Movies
SELECT cypher('CREATE (m:Movie {title: "Inception",       year: 2010, rating: 8.8})');
SELECT cypher('CREATE (m:Movie {title: "The Dark Knight", year: 2008, rating: 9.0})');
SELECT cypher('CREATE (m:Movie {title: "Interstellar",    year: 2014, rating: 8.6})');
SELECT cypher('CREATE (m:Movie {title: "Memento",         year: 2000, rating: 8.4})');
SELECT cypher('CREATE (m:Movie {title: "Dunkirk",         year: 2017, rating: 7.9})');

-- Actors
SELECT cypher('CREATE (a:Actor {name: "Leonardo DiCaprio", born: 1974})');
SELECT cypher('CREATE (a:Actor {name: "Christian Bale",    born: 1974})');
SELECT cypher('CREATE (a:Actor {name: "Tom Hardy",         born: 1977})');
SELECT cypher('CREATE (a:Actor {name: "Cillian Murphy",    born: 1976})');
SELECT cypher('CREATE (a:Actor {name: "Ken Watanabe",      born: 1959})');
SELECT cypher('CREATE (a:Actor {name: "Guy Pearce",        born: 1967})');
SELECT cypher('CREATE (a:Actor {name: "Mark Rylance",      born: 1960})');

-- Directors
SELECT cypher('CREATE (d:Director {name: "Christopher Nolan", born: 1970})');

-- ACTED_IN
SELECT cypher('MATCH (a:Actor {name: "Leonardo DiCaprio"}), (m:Movie {title: "Inception"})       CREATE (a)-[:ACTED_IN {role: "Cobb"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Ken Watanabe"}),      (m:Movie {title: "Inception"})       CREATE (a)-[:ACTED_IN {role: "Saito"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Tom Hardy"}),         (m:Movie {title: "Inception"})       CREATE (a)-[:ACTED_IN {role: "Eames"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Christian Bale"}),    (m:Movie {title: "The Dark Knight"}) CREATE (a)-[:ACTED_IN {role: "Bruce Wayne"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Tom Hardy"}),         (m:Movie {title: "The Dark Knight"}) CREATE (a)-[:ACTED_IN {role: "Bane"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Cillian Murphy"}),    (m:Movie {title: "The Dark Knight"}) CREATE (a)-[:ACTED_IN {role: "Scarecrow"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Cillian Murphy"}),    (m:Movie {title: "Inception"})       CREATE (a)-[:ACTED_IN {role: "Fischer"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Guy Pearce"}),        (m:Movie {title: "Memento"})         CREATE (a)-[:ACTED_IN {role: "Leonard"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Cillian Murphy"}),    (m:Movie {title: "Dunkirk"})         CREATE (a)-[:ACTED_IN {role: "Shivering Soldier"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Tom Hardy"}),         (m:Movie {title: "Dunkirk"})         CREATE (a)-[:ACTED_IN {role: "Farrier"}]->(m)');
SELECT cypher('MATCH (a:Actor {name: "Mark Rylance"}),      (m:Movie {title: "Dunkirk"})         CREATE (a)-[:ACTED_IN {role: "Mr. Dawson"}]->(m)');

-- DIRECTED
SELECT cypher('MATCH (d:Director {name: "Christopher Nolan"}), (m:Movie {title: "Inception"})       CREATE (d)-[:DIRECTED]->(m)');
SELECT cypher('MATCH (d:Director {name: "Christopher Nolan"}), (m:Movie {title: "The Dark Knight"}) CREATE (d)-[:DIRECTED]->(m)');
SELECT cypher('MATCH (d:Director {name: "Christopher Nolan"}), (m:Movie {title: "Interstellar"})    CREATE (d)-[:DIRECTED]->(m)');
SELECT cypher('MATCH (d:Director {name: "Christopher Nolan"}), (m:Movie {title: "Memento"})         CREATE (d)-[:DIRECTED]->(m)');
SELECT cypher('MATCH (d:Director {name: "Christopher Nolan"}), (m:Movie {title: "Dunkirk"})         CREATE (d)-[:DIRECTED]->(m)');
```

## 1. Multi-Hop Traversals

Walk across more than one relationship in a single pattern.

### Two-hop: actors who appeared in movies by the same director

```sql
SELECT
    json_extract(value, '$.actor1') AS actor1,
    json_extract(value, '$.actor2') AS actor2,
    json_extract(value, '$.movie')  AS shared_movie
FROM json_each(cypher('
    MATCH (a1:Actor)-[:ACTED_IN]->(m:Movie)<-[:ACTED_IN]-(a2:Actor)
    WHERE a1.name < a2.name
    RETURN a1.name AS actor1, a2.name AS actor2, m.title AS movie
    ORDER BY m.title, actor1
'));
```

Output:

```
actor1            actor2              shared_movie
----------------  ------------------  ------------
Christian Bale    Cillian Murphy      The Dark Knight
Christian Bale    Tom Hardy           The Dark Knight
Cillian Murphy    Tom Hardy           The Dark Knight
...
```

### Three-hop: director -> movie -> actor -> movie

Which other movies have actors from a director's film appeared in?

```sql
SELECT
    json_extract(value, '$.director') AS director,
    json_extract(value, '$.actor')    AS actor,
    json_extract(value, '$.other')    AS other_movie
FROM json_each(cypher('
    MATCH (d:Director)-[:DIRECTED]->(m:Movie)<-[:ACTED_IN]-(a:Actor)-[:ACTED_IN]->(other:Movie)
    WHERE other.title <> m.title
    RETURN DISTINCT d.name AS director, a.name AS actor, other.title AS other
    ORDER BY actor
'));
```

## 2. Variable-Length Paths

Use `[*min..max]` to traverse a variable number of hops.

### Everyone reachable from an actor within 1–3 ACTED_IN hops

```sql
SELECT
    json_extract(value, '$.reach') AS reachable_node
FROM json_each(cypher('
    MATCH (a:Actor {name: "Tom Hardy"})-[:ACTED_IN*1..2]->(m)
    RETURN DISTINCT m.title AS reach
'));
```

The pattern `[:ACTED_IN*1..2]` matches one or two consecutive ACTED_IN edges, so it finds movies Tom Hardy acted in directly, and then movies those movies lead to (if there were further ACTED_IN from Movie nodes).

### Shortest path between two actors (Bacon-number style)

Variable-length paths with any relationship type:

```sql
SELECT cypher('
    MATCH path = (a:Actor {name: "Leonardo DiCaprio"})-[*1..4]-(b:Actor {name: "Mark Rylance"})
    RETURN length(path) AS hops
    ORDER BY hops
    LIMIT 1
');
```

## 3. OPTIONAL MATCH

`OPTIONAL MATCH` returns `null` for missing parts of the pattern instead of dropping the row entirely. This is equivalent to a SQL LEFT JOIN.

### List all movies with their director (null if not yet recorded)

```sql
SELECT
    json_extract(value, '$.title')    AS title,
    json_extract(value, '$.year')     AS year,
    json_extract(value, '$.director') AS director
FROM json_each(cypher('
    MATCH (m:Movie)
    OPTIONAL MATCH (d:Director)-[:DIRECTED]->(m)
    RETURN m.title AS title, m.year AS year, d.name AS director
    ORDER BY m.year
'));
```

Output:

```
title             year  director
----------------  ----  -----------------
Memento           2000  Christopher Nolan
The Dark Knight   2008  Christopher Nolan
Inception         2010  Christopher Nolan
Interstellar      2014  Christopher Nolan
Dunkirk           2017  Christopher Nolan
```

### Count cast size, including movies with no recorded cast

```sql
SELECT
    json_extract(value, '$.title') AS title,
    json_extract(value, '$.cast')  AS cast_size
FROM json_each(cypher('
    MATCH (m:Movie)
    OPTIONAL MATCH (a:Actor)-[:ACTED_IN]->(m)
    RETURN m.title AS title, count(a) AS cast
    ORDER BY cast DESC
'));
```

Output:

```
title             cast_size
----------------  ---------
Dunkirk           3
The Dark Knight   3
Inception         3
Memento           1
Interstellar      0
```

`Interstellar` has 0 because no ACTED_IN relationships were created for it.

## 4. WITH Clause for Pipelining

`WITH` passes the results of one query stage as input to the next, similar to a SQL CTE. It lets you filter and reshape data mid-query.

### Find actors who appeared in more than one film, then get their earliest role

```sql
SELECT
    json_extract(value, '$.actor')        AS actor,
    json_extract(value, '$.film_count')   AS films,
    json_extract(value, '$.first_movie')  AS first_movie
FROM json_each(cypher('
    MATCH (a:Actor)-[:ACTED_IN]->(m:Movie)
    WITH a, count(m) AS film_count, min(m.year) AS earliest_year
    WHERE film_count > 1
    MATCH (a)-[:ACTED_IN]->(first:Movie {year: earliest_year})
    RETURN a.name AS actor, film_count, first.title AS first_movie
    ORDER BY film_count DESC
'));
```

Output:

```
actor           films  first_movie
--------------  -----  ---------------
Tom Hardy       3      The Dark Knight
Cillian Murphy  3      The Dark Knight
```

### Ranked movies by rating, then filter to top tier

```sql
SELECT
    json_extract(value, '$.title')  AS title,
    json_extract(value, '$.rating') AS rating,
    json_extract(value, '$.tier')   AS tier
FROM json_each(cypher('
    MATCH (m:Movie)
    WITH m.title AS title, m.rating AS rating
    ORDER BY rating DESC
    WITH title, rating,
         CASE
             WHEN rating >= 9.0 THEN "Masterpiece"
             WHEN rating >= 8.5 THEN "Excellent"
             ELSE "Good"
         END AS tier
    RETURN title, rating, tier
'));
```

## 5. UNWIND for List Processing

`UNWIND` flattens a list into individual rows — useful for batch creation and list comprehensions.

### Create multiple nodes from an inline list

```sql
SELECT cypher('
    UNWIND ["Action", "Thriller", "Sci-Fi", "Drama"] AS genre_name
    CREATE (g:Genre {name: genre_name})
');
```

### Batch-tag movies using UNWIND

```sql
SELECT cypher('
    UNWIND [
        {movie: "Inception",    genre: "Sci-Fi"},
        {movie: "Inception",    genre: "Action"},
        {movie: "The Dark Knight", genre: "Action"},
        {movie: "The Dark Knight", genre: "Drama"},
        {movie: "Memento",      genre: "Thriller"},
        {movie: "Dunkirk",      genre: "Drama"}
    ] AS row
    MATCH (m:Movie {title: row.movie}), (g:Genre {name: row.genre})
    CREATE (m)-[:IN_GENRE]->(g)
');
```

### Expand a collected list back to rows

```sql
SELECT
    json_extract(value, '$.genre')  AS genre,
    json_extract(value, '$.movie')  AS movie
FROM json_each(cypher('
    MATCH (m:Movie)-[:IN_GENRE]->(g:Genre)
    WITH g.name AS genre, collect(m.title) AS movies
    UNWIND movies AS movie
    RETURN genre, movie
    ORDER BY genre, movie
'));
```

## 6. Aggregation

### Count, sum, average, collect

```sql
SELECT
    json_extract(value, '$.genre')   AS genre,
    json_extract(value, '$.count')   AS film_count,
    json_extract(value, '$.avg_rating') AS avg_rating,
    json_extract(value, '$.titles')  AS titles
FROM json_each(cypher('
    MATCH (m:Movie)-[:IN_GENRE]->(g:Genre)
    RETURN g.name AS genre,
           count(m) AS count,
           round(avg(m.rating), 2) AS avg_rating,
           collect(m.title) AS titles
    ORDER BY count DESC
'));
```

Output:

```
genre    film_count  avg_rating  titles
-------  ----------  ----------  ------------------------------------
Action   2           8.9         ["Inception","The Dark Knight"]
Drama    2           8.45        ["The Dark Knight","Dunkirk"]
Sci-Fi   1           8.8         ["Inception"]
Thriller 1           8.4         ["Memento"]
```

### Top-N with ORDER BY and LIMIT

```sql
SELECT
    json_extract(value, '$.actor')  AS actor,
    json_extract(value, '$.films')  AS film_count
FROM json_each(cypher('
    MATCH (a:Actor)-[:ACTED_IN]->(m:Movie)
    RETURN a.name AS actor, count(m) AS films
    ORDER BY films DESC
    LIMIT 3
'));
```

## 7. CASE Expressions

CASE works both inline and in return clauses.

### Label movie quality tier

```sql
SELECT
    json_extract(value, '$.title')  AS title,
    json_extract(value, '$.rating') AS rating,
    json_extract(value, '$.label')  AS quality
FROM json_each(cypher('
    MATCH (m:Movie)
    RETURN m.title AS title, m.rating AS rating,
           CASE
               WHEN m.rating >= 9.0 THEN "Masterpiece"
               WHEN m.rating >= 8.5 THEN "Excellent"
               WHEN m.rating >= 8.0 THEN "Very Good"
               ELSE "Good"
           END AS label
    ORDER BY m.rating DESC
'));
```

Output:

```
title             rating  quality
----------------  ------  -----------
The Dark Knight   9.0     Masterpiece
Inception         8.8     Excellent
Interstellar      8.6     Excellent
Memento           8.4     Very Good
Dunkirk           7.9     Good
```

### Conditional aggregation

```sql
SELECT
    json_extract(value, '$.actor')          AS actor,
    json_extract(value, '$.high_rated')     AS high_rated_count,
    json_extract(value, '$.lower_rated')    AS lower_rated_count
FROM json_each(cypher('
    MATCH (a:Actor)-[:ACTED_IN]->(m:Movie)
    RETURN a.name AS actor,
           count(CASE WHEN m.rating >= 8.5 THEN 1 END) AS high_rated,
           count(CASE WHEN m.rating < 8.5  THEN 1 END) AS lower_rated
    ORDER BY actor
'));
```

## 8. UNION Queries

`UNION ALL` combines result sets, keeping duplicates. `UNION` deduplicates.

### List actors and directors in a unified "people" result

```sql
SELECT
    json_extract(value, '$.name') AS name,
    json_extract(value, '$.role') AS role
FROM json_each(cypher('
    MATCH (a:Actor)
    RETURN a.name AS name, "Actor" AS role
    UNION ALL
    MATCH (d:Director)
    RETURN d.name AS name, "Director" AS role
    ORDER BY name
'));
```

### Combine movies that were either highly rated OR recent

```sql
SELECT
    json_extract(value, '$.title')  AS title,
    json_extract(value, '$.reason') AS reason
FROM json_each(cypher('
    MATCH (m:Movie) WHERE m.rating >= 9.0
    RETURN m.title AS title, "Top rated" AS reason
    UNION
    MATCH (m:Movie) WHERE m.year >= 2014
    RETURN m.title AS title, "Recent release" AS reason
    ORDER BY title
'));
```

## 9. Working with Results via json_each()

The `cypher()` function returns a JSON array. Use SQLite's `json_each()` and `json_extract()` to integrate results with the rest of your schema.

### Join Cypher results with a regular SQL table

Suppose you have a standard `reviews` table tracking critic scores:

```sql
CREATE TABLE IF NOT EXISTS reviews (
    title TEXT PRIMARY KEY,
    critic_score REAL,
    review_count INTEGER
);

INSERT OR IGNORE INTO reviews VALUES ('Inception',       95, 290);
INSERT OR IGNORE INTO reviews VALUES ('The Dark Knight', 94, 285);
INSERT OR IGNORE INTO reviews VALUES ('Interstellar',    72, 259);
INSERT OR IGNORE INTO reviews VALUES ('Memento',         93, 180);
INSERT OR IGNORE INTO reviews VALUES ('Dunkirk',         92, 259);
```

Now join graph data with SQL data:

```sql
WITH movie_ratings AS (
    SELECT
        json_extract(value, '$.title')   AS title,
        json_extract(value, '$.audience') AS audience_score
    FROM json_each(cypher('
        MATCH (m:Movie)
        RETURN m.title AS title, m.rating AS audience
    '))
)
SELECT
    mr.title,
    mr.audience_score,
    r.critic_score,
    r.review_count,
    ROUND(ABS(mr.audience_score * 10 - r.critic_score), 1) AS gap
FROM movie_ratings mr
JOIN reviews r ON r.title = mr.title
ORDER BY gap DESC;
```

Output:

```
title          audience_score  critic_score  review_count  gap
-------------  --------------  ------------  ------------  ----
Interstellar   8.6             72            259           14.0
The Dark Knight 9.0            94            285           4.0
Inception      8.8             95            290           7.0
Dunkirk        7.9             92            259           13.0
Memento        8.4             93            180           9.0
```

### Create a SQL view over a Cypher query

```sql
CREATE VIEW IF NOT EXISTS actor_filmography AS
SELECT
    json_extract(value, '$.actor')  AS actor,
    json_extract(value, '$.movie')  AS movie,
    json_extract(value, '$.year')   AS year,
    json_extract(value, '$.role')   AS role
FROM json_each(cypher('
    MATCH (a:Actor)-[r:ACTED_IN]->(m:Movie)
    RETURN a.name AS actor, m.title AS movie, m.year AS year, r.role AS role
    ORDER BY m.year
'));
```

Use it like any table:

```sql
SELECT * FROM actor_filmography WHERE actor = 'Tom Hardy' ORDER BY year;
```

Output:

```
actor      movie             year  role
---------  ----------------  ----  -------
Tom Hardy  The Dark Knight   2008  Bane
Tom Hardy  Inception         2010  Eames
Tom Hardy  Dunkirk           2017  Farrier
```

## Next Steps

* [Graph Algorithms (SQL)](../sql-algorithms/) — Run PageRank, community detection, and path finding from SQL
* [SQL Interface Reference](../../reference/sql-interface/) — Full `cypher()` documentation and schema tables
* [Cypher Functions Reference](../../reference/cypher-functions/) — All built-in string, math, list, and aggregate functions
