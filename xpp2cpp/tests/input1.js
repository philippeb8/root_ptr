{
    var temporary = 0;
    
    var bar = function ()
    {
        return 10;
    };
    
    var foo = function ()
    {
        var object = 11;
        return function(argument) { return argument; };
    };
    
    for (var i = 0; i < 1000000; ++ i)
        console.log((foo())(temporary));
}
