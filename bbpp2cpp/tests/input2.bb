class Document
{
    auto head = nullptr<Document>();
    auto tail = nullptr<Document>();

    Document() { cout << __PRETTY_FUNCTION__ << endl; }
    ~Document() { cout << __PRETTY_FUNCTION__ << endl; }
    
    auto foo = function (int argument) { cout << __PRETTY_FUNCTION__ << endl; return argument; };
};

int main()
{
    auto temporary = 1;
    
    auto document = new Document();
    document.foo(temporary);
    
    auto bar = function ()
    {
        auto document = new Document();
        
        // cycle
        document.head = document;
        
        return document;
    };
    
    cout << 1 << endl;
    auto result = bar().foo(temporary);
    cout << 2 << endl;
}
