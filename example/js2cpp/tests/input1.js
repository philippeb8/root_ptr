{
    var temporary = 0;
    
    var bar = function (object)
    {
        return 10;
    };
    
    var foo = function ()
    {
        var object;
        var result = function() { return object; };
        return function() { return bar( object ); };
    };
    
    for (var i = 0; i < 1000000; ++ i)
        console_log((foo())(temporary));
}
