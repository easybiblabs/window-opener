click.html.gz.h: click.html.gz
	xxd -i $^ > $@
click.html.gz: click.html
	gzip -c $^ > $@

