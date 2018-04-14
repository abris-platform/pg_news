# pgnews

```
makepkg -fci
sudo systemctl restart postgresql.service
REGRESS_OPTS=--user=postgres make installcheck
```
