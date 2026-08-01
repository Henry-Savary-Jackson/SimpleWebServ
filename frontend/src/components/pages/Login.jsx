import { useContext, useState } from "react";
import { CSRFContext } from "../../providers/CSRFProvider";
import { Button, Form, FormLabel } from "react-bootstrap"
import { login } from "../../utils/RequestUtils";

function Login() {

    let [username, setUsername] = useState("")
    let [password, setPassword] = useState("")
    let csrf = useContext(CSRFContext)



    return <Form onSubmit={async (e) => {
        e.preventDefault();
        try { await login(username, password, csrf); }
        catch (e) {
            alert(`Failed to login:${e}`)
        }
    }} >
        <FormLabel htmlFor="username">Username</FormLabel>
        <Form.Control name="username" id="form-username" value={username} onChange={(e) => { setUsername(e.target.value) }} />
        <FormLabel htmlFor="pwd">Password:</FormLabel>
        <Form.Control type={"password"} id="form-pwd" value={password} onChange={(e) => { setPassword(e.target.value) }} />
        <Button disabled={!password} type="submit">Login</Button>
        <Link to={"/signup"}>No account? Sign Up</Link>
    </Form>


}

export default Login;
