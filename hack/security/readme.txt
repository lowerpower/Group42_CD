
In order get things up and running,

- You need a UNIX system to run SATAN.

- In order to unpack the SATAN archive,

	gunzip satan-11.tgz

      tar xvf satan-11.tar

- In order to unpack the SATAN documentation,

	gunzip satandoc.tgz

      tar xvf satandoc.tar

- You will need PERL 5.000 or better (perl5 alpha is NOT good enough),
  and a WWW browser (Netscape, Mosaic, or Lynx). SATAN looks a lot
  better on a color display. The FAQ gives hints for MONO screens.

- When you collect or view data about hundreds of hosts you will need a
  machine with enough CPU power (sparc5, indy, or better) and memory
  (32 MB or better).

- Run the "reconfig" script. It will patch some scripts with the
  pathnames of your PERL 5 executable, and of your WWW browser.  If
  SATAN does not find the WWW browser that you want to use, edit the
  config/paths.pl file and change the line 

	$MOSAIC="program_name";

  to whatever browser you prefer (make sure to preserve the quotation
  marks and punctuation of the line.)

- Run the "make" command. It will ask you to specify a system type.
  Most mainstream system types are provided. 

- When your network lies behind a firewall, you should unset your proxy
  environment variables (such as $http_proxy $file_proxy, $socks_ns,
  etc.) and/or change your browser configuration to not use your SOCKS
  host or HTTP Proxy (see your HTML browser's option section.)

- Run the "satan" script. When run without arguments, it will start up
  a WWW browser. The command-line interface is described in the satan.8
  manual page ("nroff -man" format). You must run SATAN as superuser if
  you want to collect data.

- You can run multiple SATAN processes in parallel to speed up data
  collection, but each process should be given its own database (via
  the "-d" command-line option). We use one SATAN database per block of
  256 addresses (satan -d x.x.x x.x.x). After data collection you can
  merge SATAN databases in core with the HTML browser (see the
  documentation on scanning and databases for more on this.) Parallel
  code and sharing of databases will come at a later date.

- Use your browser's PRINT button to print reports.

Most documentation is accessible via your WWW browser. 

Last but not least, SATAN was written to improve Internet security.
Don't put our work to shame.

	Wietse Venema / Dan Farmer 
	    (satan@fish.com)

