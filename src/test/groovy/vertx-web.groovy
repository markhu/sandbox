#!/usr/bin/env groovy

// simple web service for testing... mock Spock.groovy --by mHudson 2017.03

@Grab("io.vertx:vertx-core:3.4.1")
import io.vertx.core.Vertx

@Grab("io.vertx:vertx-auth-oauth2:3.4.1")
import io.vertx.ext.auth.oauth2.OAuth2Auth

@Grab("io.vertx:vertx-web:3.4.1")
import io.vertx.ext.web.Router
import io.vertx.ext.web.handler.StaticHandler
import io.vertx.ext.web.handler.ResponseTimeHandler
import io.vertx.ext.web.handler.LoggerFormat
import io.vertx.ext.web.handler.LoggerHandler

def port=8080
Vertx vertx = Vertx.vertx()  // implied in examples
def router = Router.router(vertx)
def route0 = router.route('/*').handler(ResponseTimeHandler.create())  // header: x-response-time
// def route0l = router.route('/*').handler(LoggerHandler.create(LoggerFormat.SHORT))

// dynamically-generated content...
def route1 = router.route('/d')
    .handler({ routingContext ->
        routingContext.response()
            .putHeader("content-type", "text/html")
            .end("Hello World!\n")
    // print "."
        })
// .order(1)

// Serve the static pages

def route2 = router.route('/*')
    .handler(StaticHandler.create("tmp")  // defaults to webroot from PWD
            .setDirectoryListing(true)
        )
// .order(2)

io.vertx.core.http.HttpServerOptions options = [host:"0.0.0.0", port:port]
def server = Vertx.vertx().createHttpServer(options)
server.requestHandler(router.&accept).listen()

println("Server is started on port ${port}")

// EOF
