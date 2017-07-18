{
    var temporary = 1;
    
    var bar = function ()
    {
        return 2;
    };
    
    var foo = function ()
    {
        return function(argument) { return argument; };
    };
    
    for (var i = 0; i < 1000000; ++ i)
        console.log((foo())(temporary) + bar());
}
