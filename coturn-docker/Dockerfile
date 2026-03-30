FROM coturn/coturn:4.9.0-r0

USER root

RUN apt-get update && \
    apt-get install -y --no-install-recommends gettext-base curl && \
    rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
