# postgresql-13.1-ml
Integration of CardEst Methods into PostgreSQL by HTTP Server


## Compare with PG CE methods
### Single Table
- test database: imdb.title
- Plan with PG CE methods:

```SQL
imdb=# explain analyze select * from title where kind_id>1 and kind_id<10;
                                                              QUERY PLAN
--------------------------------------------------------------------------------------------------------------------------------------
 Bitmap Heap Scan on title  (cost=174.01..25746.20 rows=12642 width=306) (actual time=163.325..646.420 rows=1865487 loops=1)
   Recheck Cond: ((kind_id > 1) AND (kind_id < 10))
   Heap Blocks: exact=35995
   ->  Bitmap Index Scan on kind_id_title  (cost=0.00..170.85 rows=12642 width=0) (actual time=156.207..156.208 rows=1865487 loops=1)
         Index Cond: ((kind_id > 1) AND (kind_id < 10))
 Planning Time: 1.313 ms
 Execution Time: 710.562 ms
(7 rows)
```

- Plan with learned CE methods:

```SQL
imdb=# set enable_ml_cardest=True;
SET
imdb=# explain analyze select * from title where kind_id>1 and kind_id<10;
                                                    QUERY PLAN
------------------------------------------------------------------------------------------------------------------
 Seq Scan on title  (cost=0.00..73920.58 rows=1242940 width=94) (actual time=0.037..586.408 rows=1865487 loops=1)
   Filter: ((kind_id > 1) AND (kind_id < 10))
   Rows Removed by Filter: 662825
 Planning Time: 32.463 ms
 Execution Time: 656.291 ms
(5 rows)
```

### Multi-Table Join
- test database: imdb
- Plan with PG CE methods:

```SQL
imdb=# explain analyze SELECT * FROM title t,movie_info mi,movie_info_idx mi_idx,movie_keyword mk,movie_companies mc WHERE t.id=mi.movie_id AND t.id=mk.movie_id AND t.id=mi_idx.movie_id AND t.id=mc.movie_id AND t.production_year>2008 AND mi.info_type_id=8 AND mi_idx.info_type_id=101;
                                                                            QUERY PLAN
-------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Nested Loop  (cost=85848.73..524166.12 rows=113688 width=244) (actual time=710.939..16674.121 rows=8275169 loops=1)
   Join Filter: (t.id = mk.movie_id)
   ->  Nested Loop  (cost=85848.30..406102.86 rows=63552 width=232) (actual time=709.695..9555.309 rows=289007 loops=1)
         Join Filter: (t.id = mi.movie_id)
         ->  Nested Loop  (cost=85847.87..214521.19 rows=125261 width=160) (actual time=709.655..2671.986 rows=248594 loops=1)
               ->  Hash Join  (cost=85847.44..128323.56 rows=121408 width=120) (actual time=709.617..1472.841 rows=84174 loops=1)
                     Hash Cond: (mi_idx.movie_id = t.id)
                     ->  Seq Scan on movie_info_idx mi_idx  (cost=0.00..25185.44 rows=457022 width=26) (actual time=0.041..237.047 rows=459925 loops=1)
                           Filter: (info_type_id = 101)
                           Rows Removed by Filter: 920110
                     ->  Hash  (cost=67608.95..67608.95 rows=671799 width=94) (actual time=709.389..709.390 rows=662065 loops=1)
                           Buckets: 32768  Batches: 32  Memory Usage: 2590kB
                           ->  Seq Scan on title t  (cost=0.00..67608.95 rows=671799 width=94) (actual time=0.020..517.937 rows=662065 loops=1)
                                 Filter: (production_year > 2008)
                                 Rows Removed by Filter: 1866247
               ->  Index Scan using mc_movie_id_btree_index on movie_companies mc  (cost=0.43..0.66 rows=5 width=40) (actual time=0.010..0.013 rows=3 loops=84174)
                     Index Cond: (movie_id = t.id)
         ->  Index Scan using mi_movie_id_btree_index on movie_info mi  (cost=0.43..1.49 rows=3 width=72) (actual time=0.025..0.027 rows=1 loops=248594)
               Index Cond: (movie_id = mc.movie_id)
               Filter: (info_type_id = 8)
               Rows Removed by Filter: 36
   ->  Index Scan using mk_movie_id_btree_index on movie_keyword mk  (cost=0.43..1.28 rows=46 width=12) (actual time=0.005..0.012 rows=29 loops=289007)
         Index Cond: (movie_id = mc.movie_id)
 Planning Time: 9.567 ms
 Execution Time: 17050.987 ms
(25 rows)

```

- Plan with Cardinality Truth:

```SQL
imdb=# set enable_ml_joinest=True;
SET
imdb=# explain analyze SELECT * FROM title t,movie_info mi,movie_info_idx mi_idx,movie_keyword mk,movie_companies mc WHERE t.id=mi.movie_id AND t.id=mk.movie_id AND t.id=mi_idx.movie_id AND t.id=mc.movie_id AND t.production_year>2008 AND mi.info_type_id=8 AND mi_idx.info_type_id=101;
                                                                             QUERY PLAN
--------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Hash Join  (cost=382970.46..528010.56 rows=8275169 width=244) (actual time=6065.698..13106.671 rows=8275169 loops=1)
   Hash Cond: (mk.movie_id = t.id)
   ->  Seq Scan on movie_keyword mk  (cost=0.00..69693.30 rows=4523930 width=12) (actual time=0.034..481.763 rows=4523930 loops=1)
   ->  Hash  (cost=370325.87..370325.87 rows=289007 width=232) (actual time=6060.832..6060.836 rows=289007 loops=1)
         Buckets: 16384  Batches: 32  Memory Usage: 1762kB
         ->  Nested Loop  (cost=85848.30..370325.87 rows=289007 width=232) (actual time=712.709..5786.444 rows=289007 loops=1)
               Join Filter: (t.id = mc.movie_id)
               ->  Nested Loop  (cost=85847.87..323662.38 rows=71285 width=192) (actual time=712.685..4696.928 rows=71285 loops=1)
                     ->  Hash Join  (cost=85847.44..128323.56 rows=84174 width=120) (actual time=712.639..1446.668 rows=84174 loops=1)
                           Hash Cond: (mi_idx.movie_id = t.id)
                           ->  Seq Scan on movie_info_idx mi_idx  (cost=0.00..25185.44 rows=457022 width=26) (actual time=0.024..240.036 rows=459925 loops=1)
                                 Filter: (info_type_id = 101)
                                 Rows Removed by Filter: 920110
                           ->  Hash  (cost=67608.95..67608.95 rows=671799 width=94) (actual time=712.442..712.443 rows=662065 loops=1)
                                 Buckets: 32768  Batches: 32  Memory Usage: 2590kB
                                 ->  Seq Scan on title t  (cost=0.00..67608.95 rows=671799 width=94) (actual time=0.017..523.143 rows=662065 loops=1)
                                       Filter: (production_year > 2008)
                                       Rows Removed by Filter: 1866247
                     ->  Index Scan using mi_movie_id_btree_index on movie_info mi  (cost=0.43..2.29 rows=3 width=72) (actual time=0.034..0.038 rows=1 loops=84174)
                           Index Cond: (movie_id = t.id)
                           Filter: (info_type_id = 8)
                           Rows Removed by Filter: 12
               ->  Index Scan using mc_movie_id_btree_index on movie_companies mc  (cost=0.43..0.59 rows=5 width=40) (actual time=0.010..0.013 rows=4 loops=71285)
                     Index Cond: (movie_id = mi.movie_id)
 Planning Time: 26.808 ms
 Execution Time: 13390.039 ms
(26 rows)
```
