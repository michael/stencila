FROM stencila/ubuntu-14.04

# Update system and install required system packages
RUN apt-get update
RUN apt-get install -y wget software-properties-common

# Install R
RUN add-apt-repository 'deb http://cran.us.r-project.org/bin/linux/ubuntu trusty/'
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E084DAB9
RUN apt-get update
RUN apt-get install -y r-base=3.2.0-1trusty r-base-dev=3.2.0-1trusty r-recommended=3.2.0-1trusty

# Do following install tasks in /tmp
WORKDIR /tmp

# Install Stencila R package requirements
# Installing Rcpp using 
#	RUN Rscript -e "install.packages('Rcpp',repos='http://cran.us.r-project.org')"
# fails silently for some unknown reason
RUN wget http://cran.r-project.org/src/contrib/Archive/Rcpp/Rcpp_0.11.5.tar.gz
RUN R CMD INSTALL Rcpp_0.11.5.tar.gz

# Install Stencila R package
RUN wget http://get.stenci.la/r/complete/x86_64-pc-linux-gnu/3.2/stencila_0.15.tar.gz
RUN R CMD INSTALL stencila_0.16.tar.gz

# Add Stencila CLI script to PATH
RUN Rscript -e 'require(stencila); stencila:::install_cli()'

# Add a user (creating home and specifying shell) and
# use it when running the container
RUN useradd --create-home --shell /bin/bash stencila
USER stencila
WORKDIR /home/stencila

# Add a Stencila storage directory
RUN mkdir -p /home/stencila/store
ENV STENCILA_STORES /home/stencila/store
