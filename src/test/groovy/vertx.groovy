#!/usr/bin/env groovy

// simple web service for testing... mock Spock.groovy --by mHudson 2017.03

@Grab("io.vertx:vertx-core:3.4.1")
import io.vertx.core.Vertx

def port = this.args ? this.args[0].toInteger() : 8080

io.vertx.core.http.HttpServerOptions options = [host:"0.0.0.0", port:port]
def server = Vertx.vertx().createHttpServer(options)
def hrn=0
def response_text = """
{"requests":{"48":{"k":"v"} }}
"""

server.requestHandler({ request ->
    response = request.response()
    def ct = request.headers().get("Accept")
    if ( request.headers().get("Accept") in [null,'*/*']) {
        ct = "text/plain"
    }
    response.putHeader("content-type", ct)  // "application/json")
            .end(response_text)
    println "HTTP request # ${hrn}:"
    request.headers().each{ println "    " + it }
    println "HTTP response # ${hrn}:"
    response.headers().each{ println "    " + it }
    hrn += 1
}).listen()

println "Started on port ${options.port}"
