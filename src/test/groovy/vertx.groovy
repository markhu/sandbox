#!/usr/bin/env groovy

// simple mock/stub web service  --by mHudson 2017.03
// benchmark with:  wrk -c 2 -d 4 -t 2 http://localhost:8080

@Grab("io.vertx:vertx-core:3.4.2")
import io.vertx.core.Vertx

def port = this.args ? this.args[0].toInteger() : 8080

options = [host:"0.0.0.0", port:port] as io.vertx.core.http.HttpServerOptions
def server = Vertx.vertx().createHttpServer(options)
def request_count=0

server.requestHandler({ request ->
    response = request.response()
    def ct = request.headers().get("Accept")
    if ( request.headers().get("Accept") in [null,'*/*']) {
        ct = "text/plain"
    }
    response.putHeader("content-type", ct)  // "application/json")
            .end("""
  {"requests":{"${request_count}":{"k":"v"} }}
""") // added linefeed prefix/suffix for user-friendly human-readability

    print   "HTTP request# ${request_count}: "
    request.headers().each{  print "${it}\t" }
    print "\n    response# ${request_count}: "
    response.headers().each{ print "${it}\t" }
    print "\n"
    request_count += 1
}).listen()

println "Started on port ${options.port}"

