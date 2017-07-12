#!/usr/bin/js

var temporary;
    
function bar(object)
{
    return 10;
}
    
function foo()
{
    var object;
    var result = function() { return object }
    return function() { return bar( object ) }
}
    
for (var i = 0; i < 1000000; ++ i)
    console.log((foo())(temporary));
