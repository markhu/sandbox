#!/usr/bin/env groovy

@Grab('org.apache.httpcomponents:httpclient:4.5.3')
import org.apache.http.client.methods.*;  // HttpGet HttpPost
import org.apache.http.entity.StringEntity
import org.apache.http.impl.client.DefaultHttpClient

// GET method is fairly easy...
def url = 'http://www.google.com/search?q=Groovy'
def httpResponse = new DefaultHttpClient().execute(new HttpGet(url)) 

println "URL ${url} returned length... ${httpResponse.entity.contentLength}"
def contentText = httpResponse.entity.content.text  // store self-closing stream
def ct = httpResponse.entity.contentType
println "URL ${url} returned httpResponse.statusLine: ${httpResponse.statusLine}"
// println " ... httpResponse.statusLine.statusCode: ${httpResponse.statusLine.statusCode}"

def filename = 'result.html'
// println "DEBUG: httpResponse: ${httpResponse}"
println "Writing ${contentText.length()} bytes of ${ct} to ${filename}"
new File(filename).text = contentText

// POST method is slightly more complex...
httpRequest = new HttpPost("https://jsonplaceholder.typicode.com/posts")
httpRequest.setEntity(new StringEntity("""{"userId":1,"title":"Joe","body":"Slim"}"""))
httpRequest.setHeader("Accept", "application/json")
httpRequest.setHeader("Content-type", "application/json")
httpResponse = new DefaultHttpClient().execute(httpRequest)
contentText = httpResponse.entity.content.text  // store self-closing stream
println "Received ${contentText.length()} bytes of ${httpResponse.entity.contentType} from POST"
httpResponse.allHeaders.each { println it }
println "\n${contentText}"
