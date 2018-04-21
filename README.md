# pgnews

```
makepkg -fci
sudo systemctl restart postgresql.service
REGRESS_OPTS=--user=postgres make installcheck
```

```sql
SELECT 	(unnest((news.load_feed('http://www.rosbalt.ru/feed/')).items)).title;
```
