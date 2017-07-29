{
    var temporary = 1;
    
    var foo = function ()
    {
        return function(argument) { return argument; };
    };
    
    for (var i = 0; i < 1000000; ++ i)
        console.log(foo()(temporary) * i);
}
