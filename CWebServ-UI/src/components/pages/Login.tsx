import { useContext, useState } from "react";
import { CSRFContext } from "../../providers/CSRFProvider";
import { Alert, Button, Form, FormLabel } from "react-bootstrap"
import { login } from "../../utils/RequestUtils";
import { Link, useNavigate } from "react-router-dom";
import { useCookies } from "react-cookie";

function Login() {

    let navigate = useNavigate()
    let [username, setUsername] = useState("")
    let [password, setPassword] = useState("")
    let [error, setError] = useState("")
    let csrf = useContext(CSRFContext)
    let [cookies, setCookie, removeCookie] = useCookies()

    return <Form onSubmit={async (e) => {
        e.preventDefault();
        e.stopPropagation()
        try { await login(username, password, csrf); setError(""); navigate("/main"); setCookie("username", username) }
        catch (e) {
            setError(`Failed to login:${e}`)
        }
    }} >
        {error && <Alert variant="danger">{error}</Alert>}
        <FormLabel htmlFor="username">Username</FormLabel>
        <Form.Control name="username" id="form-username" value={username} onChange={(e) => { setUsername(e.target.value) }} />
        <FormLabel htmlFor="pwd">Password:</FormLabel>
        <Form.Control type={"password"} id="form-pwd" value={password} onChange={(e) => { setPassword(e.target.value) }} />
        <Button disabled={!password} type="submit">Login</Button>
        <Link to={"/signup"}>No account? Sign Up</Link>
    </Form>


}

export default Login;
